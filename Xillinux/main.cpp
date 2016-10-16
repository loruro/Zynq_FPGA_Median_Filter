/**
  ******************************************************************************
  * @file    main.cpp
  * @author  Karol Leszczyński
  * @version V1.0.0
  * @date    27-June-2016
  * @brief   Main file of Xillinux part of Zynq_FPGA_Median_Filter program.
  *
  * This program forks into two processes:
  * First one receives frames from camera and sends them to FPGA.
  * Second one gets filtered frames from FPGA and shows them on display.
  * All operations on image are perfmored using OpenCV library.
  *
  ******************************************************************************
  * @attention
  *
  * This file is part of Zynq_FPGA_Median_Filter.
  *
  * Zynq_FPGA_Median_Filter is free software: you can redistribute it and/or
  * modify it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  *
  * Zynq_FPGA_Median_Filter is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with Zynq_FPGA_Median_Filter.
  * If not, see <http://www.gnu.org/licenses/>.
  *
  ******************************************************************************
  */

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

#include "opencv2/opencv.hpp"

using namespace cv;

const int width = 640;
const int height = 480;

int main(int argc, char *argv[]) {
  int fdr, fdw, rc, donebytes;
  pid_t pid;

  // Opening FPGA FIFOs.
  fdr = open("/dev/xillybus_read_32", O_RDONLY);
  fdw = open("/dev/xillybus_write_32", O_WRONLY);
  if ((fdr < 0) || (fdw < 0)) {
    perror("Failed to open Xillybus device file(s)");
    exit(1);
  }
  
  pid = fork();
  if (pid < 0) {
    perror("Failed to fork()");
    exit(1);
  }

  if (pid) {
    // Process which inserts camera frames into FPGA.
    
    close(fdr);
    
    namedWindow("Original", 1);
    Mat noise(height, width, CV_8U);
    Mat noise3(height, width, CV_8UC3);
    while(1) {
      // Getting frame from camera.
      VideoCapture cap(0);
      Mat frame;
      cap >> frame;
      
      // Artificially noising.
      randn(noise, 0, 0.25);
      noise *= 255;
      cvtColor(noise, noise3, CV_GRAY2RGB);
      frame = frame + noise3;

      // Inserting every RGB color separately.
      for (int color = 0; color < 3; color++) {
        for(int i = 0; i < height; i++) {
          uint8_t buffer[width];
          for(int j = 0; j < width; j++)
            buffer[j] = frame.at<Vec3b>(i, j)[color];
          
          donebytes = 0;
          while (donebytes < sizeof(buffer)) {
            rc = write(fdw, buffer + donebytes, sizeof(buffer) - donebytes);
          
            if ((rc < 0) && (errno == EINTR))
              continue;

            if (rc <= 0) {
              perror("write() failed");
              exit(1);
            }
            donebytes += rc;
          }
        }
      }
      imshow("Original", frame);
      waitKey(30);
    }
    
    close(fdw);
    return 0;
  } else {
    // Process which gets frames from FPGA and display them.
    
    close (fdw);
    
    namedWindow("Filtered", 1);
    Mat frame = Mat::zeros(height, width, CV_8UC3);
    while(1) {
      // Getting every RGB color separately.
      for(int color=0; color< 3;color++) {
        for(int i = 0; i < height; i++) {
          uint8_t buffer[width];
          donebytes = 0;
          while (donebytes < sizeof(buffer)) {
            rc = read(fdr, buffer + donebytes, sizeof(buffer) - donebytes);

            if ((rc < 0) && (errno == EINTR))
              continue;

            if (rc < 0) {
              perror("read() failed");
              exit(1);
            }

            if (rc == 0) {
              fprintf(stderr, "Reached read EOF!? Should never happen.\n");
              exit(0);
            }
            
            donebytes += rc;
          }
          for(int j = 0; j < width; j++)
            frame.at<Vec3b>(i, j)[color] = buffer[j];
        }
      }

      imshow("Filtered", frame);
      cvMoveWindow("Filtered", 350, 250);
      waitKey(30);
    }
    close(fdr);
    return 0;
  }
}
