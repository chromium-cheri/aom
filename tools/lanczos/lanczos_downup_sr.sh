#!/bin/bash
#
# Down-up script that uses super-resolution after the
# upsampling step.
#
# Usage:
#   lanczos_downup.sh [<Options>]
#                     <input_y4m>
#                     <num_frames>
#                     <horz_resampling_config>
#                     <vert_resampling_config>
#                     <downup_y4m>
#                     <downupsr_y4m>
#                     [<down_y4m>]
#
#   Notes:
#       <Options> are optional switches similar to what is used by
#           lanczos_resample_y4m utility
#       <y4m_input> is input y4m video
#       <num_frames> is number of frames to process
#       <horz_resampling_config> and <vert_resampling_config> are in format:
#               <p>:<q>:<Lanczos_a_str>[:<x0>]
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
#       <downup_y4m> is the output y4m video after upscaling.
#       <downupsr_y4m> is the output y4m video after super-resolution.
#       <down_y4m> provides the intermediate resampled file as an
#           optional parameter. If skipped the intermediate resampled
#           file is deleted.
#

set -e

tmpdir="/tmp"
AOMENC="${tmpdir}/aomenc_$$"
AOMDEC="${tmpdir}/aomdec_$$"
RESAMPLE="${tmpdir}/lanczos_resample_y4m_$$"
SUPERRES="${tmpdir}/cnn_restore_y4m_$$"
cp ./aomenc $AOMENC
cp ./aomdec $AOMDEC
cp ./lanczos_resample_y4m $RESAMPLE
cp ./cnn_restore_y4m $SUPERRES

trap 'echo "Exiting..."; rm -f ${AOMENC} ${AOMDEC} ${RESAMPLE}' EXIT

opts=0
options=""
while [[ "${@:$((opts+1)):1}" == -* ]]; do
  options="$options ${@:$((opts+1)):1}"
  ((opts=opts+1))
done
if [[ $# -lt "((opts+6))" ]]; then
  echo "Too few parameters $(($#-opts))"
  exit 1;
fi
input_y4m=${@:$((opts+1)):1}
nframes=${@:$((opts+2)):1}
hdconfig=${@:$((opts+3)):1}
vdconfig=${@:$((opts+4)):1}
downup_y4m=${@:$((opts+5)):1}
downupsr_y4m=${@:$((opts+6)):1}
down_y4m=${@:$((opts+7)):1}

#Get width and height
hdr=$(head -1 $input_y4m)
twidth=$(awk -F ' ' '{ print $2 }' <<< "${hdr}")
theight=$(awk -F ' ' '{ print $3 }' <<< "${hdr}")
width=${twidth:1}
height=${theight:1}

#Get intermediate (down) file
if [[ -z $down_y4m ]]; then
  down_y4m=/tmp/down_$$.y4m
fi

#Obtain the horizontal and vertical upsampling configs
hdconfig_arr=(${hdconfig//:/ })
haparams=${hdconfig_arr[2]}
OIFS="$IFS"; IFS='^' haparams_arr=($haparams); IFS="$OIFS"
hdconfig="${hdconfig_arr[0]}:${hdconfig_arr[1]}:${haparams_arr[0]}"
srhconfig="${hdconfig_arr[1]}:${hdconfig_arr[0]}"
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
srvconfig="${vdconfig_arr[1]}:${vdconfig_arr[0]}"
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

$RESAMPLE $options $input_y4m $nframes $hdconfig $vdconfig $down_y4m &&
$RESAMPLE $options $down_y4m $nframes $huconfig $vuconfig $downup_y4m ${width}x${height}
$SUPERRES $downup_y4m $nframes $srhconfig $downupsr_y4m

#tiny_ssim_highbd $input_y4m $downup_y4m
#tiny_ssim_highbd $input_y4m $downupsr_y4m

if [[ -z ${@:$((opts+7)):1} ]]; then
  rm $down_y4m
fi
