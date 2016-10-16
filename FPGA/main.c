/**
  ******************************************************************************
  * @file    main.c
  * @author  Karol Leszczyński
  * @version V1.0.0
  * @date    27-June-2016
  * @brief   Main file of FPGA part of Zynq_FPGA_Median_Filter program.
  *
  * It is an implementation of median filter.
  * This C program is transformed by HLS (Xilinx High-Level Synthesis) into
  * Verilog and later synthesized into FPGA configuration.
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

#include <stdint.h>

#define WIDTH 640
#define HEIGHT 480

// Bubble sort was chosen because HLS does not allow using recursive functions.
void bubblesort(uint8_t tab[9], uint8_t size) {
  for(uint8_t i = 0; i < size; i++) {
    for(uint8_t j = 0; j < size - 1; j++) {
      if (tab[j] > tab[j + 1]) {
        uint8_t tmp = tab[j];
        tab[j] = tab [j + 1];
        tab[j + 1] = tmp;
      }
    }
  }
}

// Outputs filteredRow which is row of pixels filtered by median filter using
// three rows of pixels. nRow specifies which row is center.
void medianFilter(uint8_t rows[3][WIDTH], uint8_t nRow,
                  uint8_t filteredRow[WIDTH]) {
  uint16_t i;
  uint8_t k, l;

  for(i = 0; i < WIDTH; i++) {
    uint8_t pixelValues[9];
    
    // 3x3 mask.
    for(k = 0; k < 3; k++) {
      for(l = 0; l < 3; l++) {
        
        // Finding proper index of mask row.
        int8_t indexRow = nRow - k + 1;
        if (indexRow > 2)
          indexRow = 0;
        else if (indexRow < 0)
          indexRow = 2;
        
        // Finding proper index of mask column.
        int16_t indexCol = i + l - 1;
        
        // If filtering first or last element of row then assign 0 for pixels
        // outside of image raster.
        if (indexCol > -1 && indexCol < WIDTH)
          pixelValues[k*3 + l] = rows[indexRow][indexCol];
        else
          pixelValues[k*3 + l] = 0;
      }
    }
    bubblesort(pixelValues, 9);
    
    // Median value is in the middle of array.
    filteredRow[i] = pixelValues[4];
  }
}

// Entry function.
void xillybus_wrapper(int *in, int *out) {
#pragma AP interface ap_fifo port=in
#pragma AP interface ap_fifo port=out
#pragma AP interface ap_ctrl_none port=return

  uint32_t x1, y1;
  uint16_t i, j;
  uint8_t filteredRow[WIDTH] = {0};
  uint8_t nRow = 0, n = 0, m = 0;

  // For 3x3 median filter we need 3 rows of pixels.
  uint8_t rows[3][WIDTH] = {0};

  // Getting first row of pixels. On input we receive 32 bit integer but each
  // pixel is 8 bit integer. Each input integer is split into 4 pixels.
  for(i = 0; i < WIDTH; i++) {
    if (n > 3 || i == 0) {
      n = 0;
      x1 = *in++;
    }
    
    rows[0][i] = (uint8_t)(x1 >> (n * 8));
    n++;
  }

  n = 0;
  for(j = 0; j < HEIGHT; j++) {
    nRow++;
    nRow %= 3;
    
    // If this is the last row of raster then next row should consist of zeros.
    if (j < HEIGHT - 1) {
      for(i = 0; i < WIDTH; i++) {
        if (n > 3 || i == 0) {
          n = 0;
          x1 = *in++;
        }

        rows[nRow][i] = (uint8_t)(x1 >> (n * 8));
        n++;
      }
    } else {
      for(i = 0; i < WIDTH; i++)
        rows[nRow][i] = 0;
    }
    
    medianFilter(rows, nRow, filteredRow);

    // Outputing filtered row of pixels. Same as for inputs every 4 pixels are
    // merged into one 32 bit integer.
    y1 = 0;
    for(i = 0; i < WIDTH; i++) {
      y1 |= ((uint32_t)filteredRow[i]) << (m * 8);
      m++;
      if (m > 3) {
        m = 0;
        *out++ = y1;
        y1 = 0;
      }
    }
  }
}
