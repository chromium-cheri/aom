#!/usr/bin/env python
## Copyright (c) 2019, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##
__author__ = "maggie.sun@intel.com, ryan.lei@intel.com"

import os
import Utils
import logging
import fileinput
from shutil import copyfile
from Config import LoggerName, FFMPEG, HDRToolsConfigFileTemplate, HDRConvert
from Utils import GetShortContentName, ExecuteCmd

subloggername = "VideoScaler"
loggername = LoggerName + '.' + '%s' % subloggername
logger = logging.getLogger(loggername)

def ValidAlgo_ffmpeg(algo):
    if (algo == 'bicubic' or algo == 'lanczos' or algo == 'sinc' or
        algo == 'bilinear' or algo == 'spline' or algo == 'gauss' or
        algo == 'bicublin' or algo == 'neighbor'):
        return True
    else:
        return False

#use ffmpeg to do image rescaling for yuv420 8bit
def RescaleWithFfmpeg(clip, outw, outh, algo, outfile, num, LogCmdOnly = False):
    args = " -y -s:v %dx%d -i %s -vf scale=%dx%d -c:v rawvideo -pix_fmt yuv420p" \
           " -sws_flags %s+accurate_rnd+full_chroma_int+full_chroma_inp+bitexact"\
           "+print_info -sws_dither none" \
           % (clip.width, clip.height, clip.file_path, outw, outh, algo)
    if (algo == 'lanczos'):
        args += " -param0 5 "
    args += " -frames %d %s" % (num, outfile)
    cmd = FFMPEG + args
    ExecuteCmd(cmd, LogCmdOnly)


def GenerateCfgFile(clip, outw, outh, algo, outfile, num, configpath):
    contentBaseName = GetShortContentName(clip.file_name, False)
    cfg_filename = contentBaseName + ('_Scaled_%s_%dx%d.cfg'% (algo, outw, outh))
    fmt = 1
    if (clip.fmt == '400'):
        fmt = 0
    elif (clip.fmt == '420'):
        fmt = 1
    elif (clip.fmt == '422'):
        fmt = 2
    elif (clip.fmt == '444'):
        fmt = 3

    cfgfile = os.path.join(configpath, cfg_filename)
    copyfile(HDRToolsConfigFileTemplate, cfgfile)
    fp = fileinput.input(cfgfile, inplace=1)
    for line in fp:
        if 'SourceFile=' in line:
            line = 'SourceFile="%s"\n' % clip.file_path
        if 'OutputFile=' in line:
            line = 'OutputFile="%s"\n' % outfile
        if 'SourceWidth=' in line:
            line = 'SourceWidth=%d\n' % clip.width
        if 'SourceHeight=' in line:
            line = 'SourceHeight=%d\n' % clip.height
        if 'OutputWidth=' in line:
            line = 'OutputWidth=%d\n' % outw
        if 'OutputHeight=' in line:
            line = 'OutputHeight=%d\n' % outh
        if 'ScalingMode=' in line:
            line = 'ScalingMode=3\n'
        if 'LanczosLobes=' in line:
            line = 'LanczosLobes=5\n'
        if 'SourceRate=' in line:
            line = 'SourceRate=%4.4f\n' % (float)(clip.fps_num / clip.fps_denom)
        if 'SourceChromaFormat=' in line:
            line = 'SourceChromaFormat=%d\n' % fmt
        if 'SourceBitDepthCmp0=' in line:
            line = 'SourceBitDepthCmp0=%d\n' % clip.bit_depth
        if 'SourceBitDepthCmp1=' in line:
            line = 'SourceBitDepthCmp1=%d\n' % clip.bit_depth
        if 'SourceBitDepthCmp2=' in line:
            line = 'SourceBitDepthCmp2=%d\n' % clip.bit_depth
        if 'OutputRate=' in line:
            line = 'OutputRate=%4.4f\n' % (float)(clip.fps_num / clip.fps_denom)
        if 'OutputChromaFormat=' in line:
            line = 'OutputChromaFormat=%d\n' % fmt
        if 'OutputBitDepthCmp0=' in line:
            line = 'OutputBitDepthCmp0=%d\n' % clip.bit_depth
        if 'OutputBitDepthCmp1=' in line:
            line = 'OutputBitDepthCmp1=%d\n' % clip.bit_depth
        if 'OutputBitDepthCmp2=' in line:
            line = 'OutputBitDepthCmp2=%d\n' % clip.bit_depth
        if 'NumberOfFrames=' in line:
            line = 'NumberOfFrames=%d\n' % num
        print(line, end='')
    fp.close()
    return cfgfile

def RescaleWithHDRTool(clip, outw, outh, algo, outfile, num, cfg_path,
                       LogCmdOnly = False):
    cfg_file = GenerateCfgFile(clip, outw, outh, algo, outfile, num, cfg_path)
    args = " -f %s" % cfg_file
    cmd = HDRConvert + args
    ExecuteCmd(cmd, LogCmdOnly)

def VideoRescaling(clip, num, outw, outh, outfile, algo, cfg_path,
                   LogCmdOnly = False):
    if ValidAlgo_ffmpeg(algo):
        RescaleWithHDRTool(clip, outw, outh, algo, outfile, num, cfg_path,
                           LogCmdOnly)
        #RescaleWithFfmpeg(clip, outw, outh, algo, outfile, num, LogCmdOnly)
    # add other tools for scaling here later
    else:
        logger.error("unsupported scaling algorithm.")

####################################################################################
##################### Major Functions ################################################
def GetDownScaledOutFile(clip, dnw, dnh, path, algo):
    contentBaseName = GetShortContentName(clip.file_name, False)
    actual_algo = 'None' if clip.width == dnw and clip.height == dnh else algo
    filename = contentBaseName + ('_Scaled_%s_%dx%d.y4m' % (actual_algo, dnw,
                                                              dnh))
    dnscaledout = os.path.join(path, filename)
    return dnscaledout

def GetUpScaledOutFile(clip, outw, outh, algo, path):
    actual_algo = 'None' if clip.width == outw and clip.height == outh else algo
    filename = GetShortContentName(clip.file_name, False) + \
               ('_Scaled_%s_%dx%d.y4m' % (actual_algo, outw, outh))
    upscaledout = os.path.join(path, filename)
    return upscaledout

def DownScaling(clip, num, outw, outh, path, cfg_path, algo, LogCmdOnly = False):
    dnScalOut = GetDownScaledOutFile(clip, outw, outh, path, algo)

    Utils.CmdLogger.write("::Downscaling\n")
    if (clip.width == outw and clip.height == outh):
        cmd = "copy %s %s" % (clip.file_path, dnScalOut)
        ExecuteCmd(cmd, LogCmdOnly)
    else:
        # call separate process to do the downscaling
        VideoRescaling(clip, num, outw, outh, dnScalOut, algo, cfg_path,
                       LogCmdOnly)
    return dnScalOut

def UpScaling(clip, num, outw, outh, path, cfg_path, algo, LogCmdOnly = False):
    upScaleOut = GetUpScaledOutFile(clip, outw, outh, algo, path)
    Utils.CmdLogger.write("::Upscaling\n")
    if (clip.width == outw and clip.height == outh):
        cmd = "copy %s %s" % (clip.file_path, upScaleOut)
        ExecuteCmd(cmd, LogCmdOnly)
    else:
        # call separate process to do the upscaling
        VideoRescaling(clip, num, outw, outh, upScaleOut, algo, cfg_path,
                       LogCmdOnly)
    return upScaleOut
