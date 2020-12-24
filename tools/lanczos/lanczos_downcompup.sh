#!/bin/bash
#
# Usage:
#   lanczos_downcompup.sh <input_y4m> <num_frames>
#                         <horz_resampling_config>
#                         <vert_resampling_config>
#                         <cq_level>[:<cpu_used>]
#                         <downcompup_y4m>
#                         [[<down_y4m>]:[<downcomp_bit>]:[<downcomp_y4m]]
#
#   Notes:
#       <y4m_input> is input y4m video
#       <num_frames> is number of frames to process
#       <horz_resampling_config> and <vert_resampling_config> are in format:
#               <p>:<q>:<Lanczos_a_str>[:<x0>:<ext>]
#           similar to what is used by lanczos_resample_y4m utility, with the
#           enhancement that for <Lanczos_a_str> optionally
#           two '^'-separated strings for 'a' could be provided instead
#           of one for down and up-sampling operations respectively if different.
#           Note each string separated by '^' could have two values for luma
#           and chroma separated by ','.
#           So <Lanczos_a_str> could be of the form:
#               <a_luma_down>,<a_chroma_down>^<a_luma_up>,<a_chroma_up>
#             if down and up operations use different parameters, or
#               <a_luma_down>,<a_chroma_down>
#             if down and up operations use the same parameters.
#       <cq_level>[:<cpu_used>] provides the cq_level parameter of
#           compression along with an optional cpu_used parameter.
#       <downcompup_y4m> is the output y4m video.
#       The last param [[<down_y4m>]:[<downcomp_bit>]:[<downcomp_y4m]]
#           provides names of intermediate files where:
#               <down_y4m> is the resampled source
#               <downcomp_bit> is the compressed resampled bitstream
#               <downcomp_y4m> is the reconstructed bitstream.
#           This parameter string is entirely optional.
#           Besides if provided, each of <down_y4m>, <downcomp_bit> and
#           <downcomp_y4m> are optional by themselves where each can be
#           either provided or empty. If empty the corresponding
#           intermediate file is deleted.
#

set -e

if [[ $# -lt "6" ]]; then
  echo "Too few parameters $#"
  exit 1;
fi

tmpdir="/tmp"
AOMENC="${tmpdir}/aomenc_$$"
AOMDEC="${tmpdir}/aomdec_$$"
RESAMPLE="${tmpdir}/lanczos_resample_y4m_$$"
cp ./aomenc $AOMENC
cp ./aomdec $AOMDEC
cp ./lanczos_resample_y4m $RESAMPLE

trap 'echo "Exiting..."; rm -f ${AOMENC} ${AOMDEC} ${RESAMPLE}' EXIT

extra=""

input_y4m=$1
nframes=$2
hdconfig=$3
vdconfig=$4
codecparams=$5
downcompup_y4m=$6
intfiles=$7

#Get codec params cq_level and cpu_used
OIFS="$IFS"; IFS=':' codecparams_arr=($codecparams); IFS="$OIFS"
cq_level=${codecparams_arr[0]}
cpu_used=${codecparams_arr[1]}
if [[ -z ${cpu_used} ]]; then
  cpu_used="0"
fi
echo "cq_level: ${cq_level}"
echo "cpu_used: ${cpu_used}"

#Get width and height
hdr=$(head -1 $input_y4m)
twidth=$(awk -F ' ' '{ print $2 }' <<< "${hdr}")
theight=$(awk -F ' ' '{ print $3 }' <<< "${hdr}")
width=${twidth:1}
height=${theight:1}

#Parse the intermediate files parameter
OIFS="$IFS"; IFS=':' intfiles_arr=($intfiles); IFS="$OIFS"
down_y4m=${intfiles_arr[0]}
downcomp_bit=${intfiles_arr[1]}
downcomp_y4m=${intfiles_arr[2]}
if [[ -z ${down_y4m} ]]; then
  down_y4m=${tmpdir}/down_$$.y4m
fi
if [[ -z ${downcomp_bit} ]]; then
  downcomp_bit=${tmpdir}/downcomp_$$.bit
fi
if [[ -z ${downcomp_y4m} ]]; then
  downcomp_y4m=${tmpdir}/downcomp_$$.y4m
fi

#Obtain the horizontal and vertical upsampling configs
hdconfig_arr=(${hdconfig//:/ })
haparams=${hdconfig_arr[2]}
OIFS="$IFS"; IFS='^' haparams_arr=($haparams); IFS="$OIFS"
hdconfig="${hdconfig_arr[0]}:${hdconfig_arr[1]}:${haparams_arr[0]}"
if [[ -z ${haparams_arr[1]} ]]; then
  huconfig="${hdconfig_arr[1]}:${hdconfig_arr[0]}:${haparams_arr[0]}"
else
  huconfig="${hdconfig_arr[1]}:${hdconfig_arr[0]}:${haparams_arr[1]}"
fi
if [[ -n ${hdconfig_arr[3]} ]]; then
  hdconfig="${hdconfig}:${hdconfig_arr[3]}"
  huconfig="${huconfig}:i${hdconfig_arr[3]//,/,i}"
fi
if [[ -n ${hdconfig_arr[4]} ]]; then
  hdconfig="${hdconfig}:${hdconfig_arr[4]}"
  huconfig="${huconfig}:${hdconfig_arr[4]}"
fi

vdconfig_arr=(${vdconfig//:/ })
vaparams=${vdconfig_arr[2]}
OIFS="$IFS"; IFS='^' vaparams_arr=($vaparams); IFS="$OIFS"
vdconfig="${vdconfig_arr[0]}:${vdconfig_arr[1]}:${vaparams_arr[0]}"
if [[ -z ${vaparams_arr[1]} ]]; then
  vuconfig="${vdconfig_arr[1]}:${vdconfig_arr[0]}:${vaparams_arr[0]}"
else
  vuconfig="${vdconfig_arr[1]}:${vdconfig_arr[0]}:${vaparams_arr[1]}"
fi
if [[ -n ${vdconfig_arr[3]} ]]; then
  vdconfig="${vdconfig}:${vdconfig_arr[3]}"
  vuconfig="${vuconfig}:i${vdconfig_arr[3]//,/,i}"
fi
if [[ -n ${vdconfig_arr[4]} ]]; then
  vdconfig="${vdconfig}:${vdconfig_arr[4]}"
  vuconfig="${vuconfig}:${vdconfig_arr[4]}"
fi

#Downsample
$RESAMPLE $input_y4m $nframes $hdconfig $vdconfig $down_y4m

#Compress
$AOMENC -o $downcomp_bit $down_y4m \
        --codec=av1 --good --threads=0 --passes=1 --lag-in-frames=19 \
        --kf-max-dist=65 --kf-min-dist=0 --test-decode=warn -v --psnr \
        --end-usage=q \
        --cq-level=${cq_level} \
        --cpu-used=${cpu_used} \
        --limit=${nframes} \
        ${extra}
$AOMDEC --progress -S --codec=av1 -o $downcomp_y4m $downcomp_bit

#Upsample
$RESAMPLE $downcomp_y4m $nframes $huconfig $vuconfig $downcompup_y4m \
          ${width}x${height}

#Compute metrics
#tiny_ssim_highbd $input_y4m $downcompup_y4m

if [[ -z ${intfiles_arr[0]} ]]; then
  rm $down_y4m
fi
if [[ -z ${intfiles_arr[1]} ]]; then
  rm $downcomp_bit
fi
if [[ -z ${intfiles_arr[2]} ]]; then
  rm $downcomp_y4m
fi
