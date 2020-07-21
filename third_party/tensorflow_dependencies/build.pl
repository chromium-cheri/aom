#!/usr/bin/env perl
#
# Copyright (c) 2020, Alliance for Open Media. All rights reserved
#
# This source code is subject to the terms of the BSD 2 Clause License and
# the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
# was not distributed with this source code in the LICENSE file, you can
# obtain it at www.aomedia.org/license/software. If the Alliance for Open
# Media Patent License 1.0 was not distributed with this source code in the
# PATENTS file, you can obtain it at www.aomedia.org/license/patent.
#
###########################################################################
#
# Script to build the static TensorFlow lite library.
#
# Tensorflow's build process generates the static libraries in the same
# directory as the source code. AOM, however, generates binaries/objects/etc.
# in a different directory. This script:
#
# 1.) Copies the TensorFlow code to a temporary directory
# 2.) Copies the necessary dependencies into the temporary directory
# 3.) Compiles it
# 4.) Copies the static library to the AOM build directory
#
# Note that we do not use the download_dependencies.sh script directly, as
# it downloads directly into the source directory.

use strict;
use warnings;
use autodie;
use Cwd;
use File::Basename;
use File::Copy;
use File::Spec::Functions;
use File::Temp;

# Apply a tweak to Eigen (TF Lite's "download_dependencies.sh"
# performs 3 tweaks, but only 1 of them is relevant).
sub apply_eigen_tweak {
    my $downloads_dir = $_[0];
    my $file = catfile($downloads_dir, "eigen", "Eigen", "src", "Core",
                       "arch", "NEON", "Complex.h");
    open(my $fh, '<', $file);
    my @lines = <$fh>;
    close($fh);
    open($fh, '>', $file);
    foreach my $line (@lines) {
        $line =~ s/static uint64x2_t p2ul_CONJ_XOR = vld1q_u64\( p2ul_conj_XOR_DATA \);/static uint64x2_t p2ul_CONJ_XOR;/;
        print $fh $line;
    }
    close($fh);
}

# Pure Perl implementation of recursive directory-copy -
# File::Copy::Recursive is not part of core.
sub recursive_dircopy {
    my $input_dir = $_[0];
    my $output_dir = $_[1];
    # If the output directory does not exist, create it.
    if (!(-d $output_dir)) {
        mkdir $output_dir;
    }
    opendir(my $dh, $input_dir);
    while (my $file = readdir($dh)) {
        # Ignore . and .. entries.
        next if ($file eq "." or $file eq "..");
        my $in_file = catfile($input_dir, $file);
        my $out_file = catfile($output_dir, $file);
        # If it is a directory, recursively call.
        if (-d $in_file) {
            recursive_dircopy($in_file, $out_file);
        } else {
          # If a file, copy over.
          copy($in_file, $out_file);
          # Preserve execute permission.
          if (-x $in_file) {
            chmod 0755, $out_file;
          }
        }
    }
    closedir($dh);
}

sub copy_tensorflow_lite_dependencies {
    my $source_dir = $_[0];
    my $output_dir = $_[1];
    my $dependencies_dir = catfile($source_dir, "third_party",
                                   "tensorflow_dependencies");
    opendir(my $dh, $dependencies_dir);
    while (my $file = readdir($dh)) {
        # Ignore . and .. entries.
        next if ($file eq "." or $file eq "..");
        # Ignore non-directories.
        next unless (-d catfile($dependencies_dir, $file));
        # Copy the directory.
        print "  * $file\n";
        recursive_dircopy(
          catfile($dependencies_dir, $file),
          catfile($output_dir, $file));
    }
    closedir($dh);
}

# Start of program logic.
if ($#ARGV + 1 != 2) {
    my $prog = basename($0);
    warn("Usage: $prog <source directory> <output directory>\n");
    exit(1);
}
my ($source_dir, $output_dir) = @ARGV;

my $temp_dir = File::Temp::tempdir( CLEANUP => 0 );

print "Copying TensorFlow code to temporary directory for building...\n";
my $tf_dir = catfile($source_dir, "third_party", "tensorflow");
recursive_dircopy($tf_dir, $temp_dir);

my $downloads_dir = catfile($temp_dir, "tensorflow", "lite", "tools",
                            "make", "downloads");
mkdir $downloads_dir;
print "Copying TensorFlow Lite dependencies to temporary directory...\n";
copy_tensorflow_lite_dependencies($source_dir, $downloads_dir);
apply_eigen_tweak($downloads_dir);

print "Building TensorFlow Lite...\n";
my $build_sh = catfile($temp_dir, "tensorflow", "lite", "tools", "make",
                       "build_lib.sh");
`$build_sh`;
my $liblite = catfile($temp_dir, "tensorflow", "lite", "tools", "make", "gen",
                      "linux_x86_64", "lib", "libtensorflow-lite.a");
print "Copying static library into build directory...\n";
copy($liblite, $output_dir);
