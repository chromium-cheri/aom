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
import sys
import argparse
from CalculateQualityMetrics import CalculateQualityMetric, GatherQualityMetrics
from Utils import GetShortContentName, CreateNewSubfolder, SetupLogging, \
     Cleanfolder, CreateClipList
import Utils
from Config import LogLevels, FrameNum, TEST_CONFIGURATIONS, QPs, WorkPath, \
     Path_RDResults, LoggerName,QualityList
from EncDecUpscale import Encode, Decode

###############################################################################
##### Helper Functions ########################################################
def CleanIntermediateFiles():
    folders = [Path_DecodedYuv, Path_CfgFiles]
    for folder in folders:
        Cleanfolder(folder)

def GetRDResultCsvFile(EncodeMethod, CodecName, EncodePreset, test_cfg):
    filename = "RDResults_%s_%s_%s_Preset_%s.csv" % \
               (EncodeMethod, CodecName, test_cfg, EncodePreset)
    file = os.path.join(Path_RDResults, filename)
    return file

def GetBsReconFileName(EncodeMethod, CodecName, EncodePreset, test_cfg, clip, QP):
    basename = GetShortContentName(clip.file_name, False)
    filename = "%s_%s_%s_%s_Preset_%s_QP_%d.ivf" % \
               (basename, EncodeMethod, CodecName, test_cfg, EncodePreset, QP)
    bs = os.path.join(Path_Bitstreams, filename)
    filename = "%s_%s_%s_%s_Preset_%s_QP_%d_Decoded.y4m" % \
               (basename, EncodeMethod, CodecName, test_cfg, EncodePreset, QP)
    dec = os.path.join(Path_DecodedYuv, filename)
    return bs, dec


def setupWorkFolderStructure():
    global Path_Bitstreams, Path_DecodedYuv, Path_QualityLog, Path_TestLog,\
           Path_CfgFiles
    Path_Bitstreams = CreateNewSubfolder(WorkPath, "bistreams")
    Path_DecodedYuv = CreateNewSubfolder(WorkPath, "decodedYUVs")
    Path_QualityLog = CreateNewSubfolder(WorkPath, "qualityLogs")
    Path_TestLog = CreateNewSubfolder(WorkPath, 'testLogs')
    Path_CfgFiles = CreateNewSubfolder(WorkPath, "configFiles")

###############################################################################
######### Major Functions #####################################################
def CleanUp_workfolders():
    folders = [Path_Bitstreams, Path_DecodedYuv, Path_QualityLog,
               Path_TestLog, Path_CfgFiles]
    for folder in folders:
        Cleanfolder(folder)

def Run_Encode_Test(test_cfg, clip, preset, LogCmdOnly = False):
    Utils.Logger.info("start running %s encode tests with %s"
                      % (test_cfg, clip.file_name))

    for QP in QPs:
        Utils.Logger.info("start encode with QP %d" % (QP))
        #encode
        if LogCmdOnly:
            Utils.CmdLogger.write("============== Job Start =================n")
        bsFile = Encode('aom', 'av1', preset, clip, test_cfg, QP, FrameNum,
                        Path_Bitstreams, LogCmdOnly)
        Utils.Logger.info("start decode file %s" % os.path.basename(bsFile))
        #decode
        decodedYUV = Decode('av1', bsFile, Path_DecodedYuv, LogCmdOnly)
        #calcualte quality distortion
        Utils.Logger.info("start quality metric calculation")
        CalculateQualityMetric(clip.file_path, FrameNum, decodedYUV, clip.fmt,
                               clip.width, clip.height, clip.bit_depth,
                               Path_QualityLog, LogCmdOnly)
        if SaveMemory:
            Cleanfolder(Path_DecodedYuv)
        Utils.Logger.info("finish running encode with QP %d" % (QP))
        if LogCmdOnly:
            Utils.CmdLogger.write("============== Job End ===================\n\n")

def GenerateSummaryRDDataFile(EncodeMethod, CodecName, EncodePreset,
                              test_cfg, clip_list):
    Utils.Logger.info("start saving RD results to excel file.......")
    if not os.path.exists(Path_RDResults):
        os.makedirs(Path_RDResults)

    csv_file = GetRDResultCsvFile(EncodeMethod, CodecName, EncodePreset, test_cfg)
    csv = open(csv_file, 'wt')
    csv.write("File,Class,Width,Height,TestCfg,EncodeMethod,CodecName,"\
              "EncodePreset,QP,Bitrate(kbps)")
    for qty in QualityList:
        csv.write(',' + qty)
    csv.write('\n')

    for clip in clip_list:
        for qp in QPs:
            bs, dec = GetBsReconFileName(EncodeMethod, CodecName, EncodePreset,
                                         test_cfg, clip, qp)
            bitrate = (os.path.getsize(bs) * 8 * (clip.fps_num / clip.fps_denom)
                       / FrameNum) / 1000.0
            quality = GatherQualityMetrics(dec, Path_QualityLog)
            csv.write("%s,%s,%d,%d,%s,%s,%s,%s,%d,%.4f"
                      %(clip.file_name, clip.file_class, clip.width, clip.height,
                      test_cfg, EncodeMethod, CodecName,EncodePreset,qp,bitrate))
            for qty in quality:
                csv.write(",%.4f"%qty)
            csv.write("\n")

    Utils.Logger.info("finish export RD results to file.")
    return

def ParseArguments(raw_args):
    parser = argparse.ArgumentParser(prog='AV2CTCTestTest.py',
                                     usage='%(prog)s [options]',
                                     description='')
    parser.add_argument('-f', '--function', dest='Function', type=str,
                        required=True, metavar='',
                        choices=["clean", "encode", "summary"],
                        help="function to run: clean, encode, summary")
    parser.add_argument('-s', "--SaveMemory", dest='SaveMemory', type=bool,
                        default=False, metavar='',
                        help="save memory mode will delete most files in"
                             " intermediate steps and keeps only necessary "
                             "ones for RD calculation. It is false by default")
    parser.add_argument('-CmdOnly', "--LogCmdOnly", dest='LogCmdOnly', type=bool,
                        default=False, metavar='',
                        help="LogCmdOnly mode will only capture the command sequences"
                             "It is false by default")
    parser.add_argument('-l', "--LoggingLevel", dest='LogLevel', type=int,
                        default=3, choices=range(len(LogLevels)), metavar='',
                        help="logging level: 0:No Logging, 1: Critical, 2: Error,"
                             " 3: Warning, 4: Info, 5: Debug")
    parser.add_argument('-p', "--EncodePreset", dest='EncodePreset', type=str,
                        metavar='', help="EncodePreset: 0,1,2... for aom")
    if len(raw_args) == 1:
        parser.print_help()
        sys.exit(1)
    args = parser.parse_args(raw_args[1:])

    global Function, SaveMemory, LogLevel, EncodePreset, LogCmdOnly
    Function = args.Function
    SaveMemory = args.SaveMemory
    LogLevel = args.LogLevel
    EncodePreset = args.EncodePreset
    LogCmdOnly = args.LogCmdOnly

######################################
# main
######################################
if __name__ == "__main__":
    #sys.argv = ["", "-f", "encode", "-p","6"]
    #sys.argv = ["", "-f", "summary", "-p", "6"]
    ParseArguments(sys.argv)

    # preparation for executing functions
    setupWorkFolderStructure()
    if Function != 'clean':
        SetupLogging(LogLevel, LogCmdOnly, LoggerName, Path_TestLog)
        clip_list = CreateClipList('RA')

    # execute functions
    if Function == 'clean':
        CleanUp_workfolders()
    elif Function == 'encode':
        for test_cfg in TEST_CONFIGURATIONS:
            for clip in clip_list:
                Run_Encode_Test(test_cfg, clip, EncodePreset, LogCmdOnly)
    elif Function == 'summary':
        for test_cfg in TEST_CONFIGURATIONS:
            GenerateSummaryRDDataFile('aom', 'av1', EncodePreset,
                                      test_cfg, clip_list)
        Utils.Logger.info("RD data summary file generated")
    else:
        Utils.Logger.error("invalid parameter value of Function")
