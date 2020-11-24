
#include "led-matrix.h"

#include "pixel-mapper.h"

#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <algorithm>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

using std::min;
using std::max;

#define TERM_ERR  "\033[1;31m"
#define TERM_NORM "\033[0m"

using namespace rgb_matrix;

volatile bool interrupt_received = false;
static void InterruptHandler(int signo) {
  interrupt_received = true;
}

class DemoRunner {
protected:
  DemoRunner(Canvas *canvas) : canvas_(canvas) {}
  inline Canvas *canvas() { return canvas_; }

public:
  virtual ~DemoRunner() {}
  virtual void Run() = 0;

private:
  Canvas *const canvas_;
};

class VolumeBars : public DemoRunner {
public:
  VolumeBars(Canvas *m, int delay_ms=50, int numBars=8)
    : DemoRunner(m), delay_ms_(delay_ms),
      numBars_(numBars) {
  }

  ~VolumeBars() {
    delete [] barHeights_;
    delete [] barFreqs_;
    delete [] barMeans_;
  }

  void Run() override {
    const int width = canvas()->width();
    height_ = canvas()->height();
    barWidth_ = width/numBars_;
    barHeights_ = new int[numBars_];
    barMeans_ = new int[numBars_];
    barFreqs_ = new int[numBars_];
    heightGreen_  = height_*4/12;
    heightYellow_ = height_*8/12;
    heightOrange_ = height_-1;
    heightRed_    = height_*12/12;
    
    // Bars falling animation properties
    int fallingStep = 1;
    int previousBarHeights[numBars_];
    
    for (int i=0; i<numBars_; ++i) {
      previousBarHeights[i] = 0;
    }
    
    // Initialize socket
    const int elCount = 32;
    const int multy = 3;
    int intMas[32];
  
    int sockfd = 0, n = 0;
    char recvBuff[elCount * multy];
    struct sockaddr_in serv_addr;
    const char* ip = "127.0.0.1";
  
    memset(recvBuff, '0',sizeof(recvBuff));
  
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
      printf("\n Error : Could not create socket \n");
      return;
    }
  
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(1488);
  
    if(inet_pton(AF_INET, ip, &serv_addr.sin_addr)<=0)
    {
      printf("\n inet_pton error occured\n");
      return;
    }
  
    if( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
      printf("\n Error : Connect Failed \n");
      return;
    }
    
    const char* c = new char[1];
    
    // Start the loop
    while (!interrupt_received) {
      
      // Send request to listener
      send(sockfd, c, 1, 0);
      
      // Read bar heights from socket
      if ((n = read(sockfd, recvBuff, sizeof(recvBuff))) <= 0) //get data from listener
      {
        continue;
      }
      
      int tmp = 0;
      int intCounter = 0;
      bool space = false;
  
      for (int i = 0; i < elCount*multy; i++) //parse data to int
      {
        if (recvBuff[i] == ' ')
        {
          if(!space)
          {
            intMas[intCounter] = tmp;
            intCounter++;
            space = true;
            tmp = 0;
            if(intCounter >= elCount)
            {
              break;
            }
          }
          continue;
        }
        if (recvBuff[i] >= '0' && recvBuff[i] <= '9')
        {
          tmp = tmp * 10 + (recvBuff[i]-'0');
        }
        space = false;
      }
      
      // Compute bar heights
      for (int i=0; i<numBars_; ++i) {
        if (intMas[i] >= previousBarHeights[i]) {
          barHeights_[i] = intMas[i];
        } else {
          barHeights_[i] = previousBarHeights[i] - fallingStep;
        }
        if (barHeights_[i] < 0) {
          barHeights_[i] = 0;
        }
      }

      // Apply bar heights
      for (int i=0; i<numBars_; ++i) {
        int y;
        for (y=0; y<barHeights_[i]; ++y) {
          if (y<heightGreen_) {
            drawBarRow(i, y, 0, 0, 255);
          }
          else if (y<heightYellow_) {
            drawBarRow(i, y, 153, 51, 255);
          }
          else if (y<heightOrange_) {
            drawBarRow(i, y, 255, 0, 255);
          }
          else {
            drawBarRow(i, y, 200, 0, 0);
          }
        }
        
        // Anything above the bar should be black
        for (; y<height_; ++y) {
          drawBarRow(i, y, 0, 0, 0);
        }
      }
    
      // Save bar heights for next iteration
      for (int i=0; i<numBars_; ++i) {
        previousBarHeights[i] = barHeights_[i];
      }
      
      usleep(delay_ms_ * 1000);
    }
    close(sockfd);
  }

private:
  void drawBarRow(int bar, int y, uint8_t r, uint8_t g, uint8_t b) {
    for (int x=bar*barWidth_; x<(bar+1)*barWidth_; ++x) {
      canvas()->SetPixel(x, height_-1-y, r, g, b);
    }
  }

  int delay_ms_;
  int numBars_;
  int* barHeights_;
  int barWidth_;
  int height_;
  int heightGreen_;
  int heightYellow_;
  int heightOrange_;
  int heightRed_;
  int* barFreqs_;
  int* barMeans_;
};

int main(int argc, char *argv[]) {
  RGBMatrix::Options matrix_options;
  rgb_matrix::RuntimeOptions runtime_opt;

  // These are the defaults when no command-line flags are given.
  matrix_options.rows = 32;
  matrix_options.chain_length = 1;
  matrix_options.parallel = 1;

  // First things first: extract the command line flags that contain
  // relevant matrix options.
  if (!ParseOptionsFromFlags(&argc, &argv, &matrix_options, &runtime_opt))
    return 1;

  RGBMatrix *matrix = RGBMatrix::CreateFromOptions(matrix_options, runtime_opt);
  if (matrix == NULL)
    return 1;

  printf("Size: %dx%d. Hardware gpio mapping: %s\n",
         matrix->width(), matrix->height(), matrix_options.hardware_mapping);

  Canvas *canvas = matrix;

  DemoRunner *demo_runner = NULL;
  
  demo_runner = new VolumeBars(canvas, 5, canvas->width()/2);
  
  if (demo_runner == NULL)
    return 1;

  // Set up an interrupt handler to be able to stop animations while they go
  // on. Each demo tests for while (!interrupt_received) {},
  // so they exit as soon as they get a signal.
  signal(SIGTERM, InterruptHandler);
  signal(SIGINT, InterruptHandler);

  printf("Press <CTRL-C> to exit and reset LEDs\n");

  // Now, run our particular demo; it will exit when it sees interrupt_received.
  demo_runner->Run();

  delete demo_runner;
  delete canvas;

  printf("\nReceived CTRL-C. Exiting.\n");
  return 0;
}
