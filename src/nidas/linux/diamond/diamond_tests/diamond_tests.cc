// 
//  diamond_tests.cc
//  NIDAS
//  
//  Created by Ryan Orendorff on 2010-06-28.
//  Copyright 2010 UCAR/NCAR. All rights reserved.
// 
//  Added in SVN Trunk Revision 5572 


#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>


#include <nidas/linux/diamond/diamond_tests/diamond_tests.h>
#include <nidas/linux/a2d.h>
#include <nidas/linux/diamond/dmd_mmat.h>
 


void printhelp(char * program_name)
{
  printf("\nusage: %s [-h] [-s MODE] [--D2D] [--default] [-e]\n",
                                                               program_name);
  printf("Used to control a Diamond I/O card.\n\n");
  printf("Options\tNotes\n");
  printf("-------\t---------------------------\n");
  // printf("     -r\tRead from register address (0-15).\n");
  // printf("       \t[ARGS] = ADDRESS\n");
  // printf("     -w\tWrite to register address (0-15) with value(0-255).\n");
  // printf("       \t[ARGS] = ADDRESS VALUE\n");
  printf("     -s\tStart Diamond in MODE.\n");
	printf("       \t\tdefault - Run with A2D set to constantly sample the\n"
								"\t\t          input to channel 0.\n");
	printf("       \t\tD2D     - Run with A2D sampling at the same rate that\n"
                "\t\t          the D2A is outputting a waveform.\n");
	printf("       \t\tmanual  - Sends a waveform out through IOCTL methods,\n"
	              "\t\t          not via the internal clock.\n");
	printf("       \t\tcounter - Starts counter 1/2 to a specified preset.\n");
	printf("     -e\tStop diamond D2A output, reset waveform buffer.\n");
  printf("     -h\tPrint help.\n");
}

int check_options(int argc, char * argv[], struct dmd_command* send)
{
  char c;
  
  char* program_name = *argv;
  char* switches;
  int break_outer = 0;
  
  if(argc == 1){
    printhelp(program_name);
    return 0;
  } 
  

  while (--argc) {
    break_outer = 0;
    switch (**++argv) { // dereference first char of next argument
      case '-':
        switches = *argv;
        while(c = *++switches){
          switch (c) {
            case '-':
              switches++;
              if (strcmp(switches, "D2D") == 0) {
                flags.mode  = 's';
                flags.smode = '2';
                break_outer =  1;
              } else if (strcmp(switches, "D2A") == 0){
								flags.mode  = 's';
								flags.smode = 'd';
								break_outer = 1;
							}
								else if (strcmp(switches, "A2D") == 0) {
                flags.mode  = 's';
                flags.smode = 'a';
                break_outer =  1;
              }
              break;

            case 'r':
              
              flags.mode = 'r';
              flags.total_set++;
              
              if (argc < 2){
                printf("Too few arguments for -r option "
                     "(address)\n\n");
                printhelp(program_name);
                return 1;
              } 
              argv++; // put here to allow atoi check of number
              
              if (atoi(*argv) > 15 || atoi(*argv) < 0){
                printf("Must send a base + addr between "
                     "0 and 15\n");
                printhelp(program_name);
                return 3;
              } 
              
              send->addr = atoi(*argv);
              argc--;
              break;
              
            case 'w':
              
              flags.mode = 'w';
              flags.total_set++;
              
              if (argc < 3){
                printf("Too few arguments for -w option "
                     "(address value)\n\n");
                printhelp(program_name);
                return 1;
              }
              argv++;
              
              if (atoi(*argv) > 15 || atoi(*argv) < 0){
                printf("Must send a base + addr between "
                     "0 and 15\n");
                printhelp(program_name);
                return 3;
              } 
              
              send->addr = atoi(*argv);
              
              argv++; // put here to allow atoi check of number
              argc--; // put here to allow atoi check of number
              
              
              if (atoi(*argv) > 255 || atoi(*argv) < 0){
                printf("Must send a value between 0 and 255\n");
                printhelp(program_name);
                return 4;
              } 
              send->value = atoi(*argv);
              
              argc--;
              break;
              
            case 's':
              
              flags.mode = 's';
              flags.total_set++;

                            
              if (argc < 2){
                printf("Too few arguments for -s option "
                     "(mode)\n\n");
                printhelp(program_name);
                return 1;
              } 
                            
              argv++;

              if (strcmp(*argv,"D2D") == 0){
                flags.smode = '2';
              }
              else if (strcmp(*argv,"A2D") == 0){
                flags.smode = 'a';
              }
							else if (strcmp(*argv,"D2A") == 0){
								flags.smode = 'd';
							}
							else {
								printf("Improper mode. Must be A2D/D2A/D2D\n");
								return 6;
							}
              return 0;
              break;
              
            case 'h':
              
              printhelp(program_name);
              return 5;
              break;
            
						case 'e':
							
							flags.mode = 's';
							flags.smode = 'j';
							break;
            default :
              printf("Illegal option specified (%c)\n", c);
              printhelp(program_name);
              return 5;
              break;
          } // end switch(c)
          
          if (break_outer) { // Basically instead of a goto function. 
            break;
          }
          
        } // end while(c = *++(*switches))
        break;
        
      default:
        
        break;
    }// end switch(**++argv)
    
  }
  
  return 0;
} // end check_options(int argc, char * argv[], struct dmd_command* send)



int read_addr(struct dmd_command* send)
{
  // struct dmd_command read_value;

  return 0;
}

int write_addr(struct dmd_command* send)
{
  // struct dmd_command write_value;

  return 0;
}

int start(char smode)
{
  switch (smode){
    case '2':
    {
      printf("Running D2D IOCTL commands.\n");
      const char* devname = "/dev/dmmat_d2d0";
      int fd = open(devname,O_RDWR);
			int res;
			int i;
			int size = 512;
			
			struct waveform *wave, *wave2, *wave3;
			int waveform[size], waveform2[size], waveform3[size];

			struct D2D_Config cfg = {1, 50};

			if ((res = ioctl(fd,DMMAT_D2D_CONFIG, &cfg)) < 0) perror(devname);
			
      for(i = 0; i < size; i++){
          waveform[i] = i*7;
					waveform2[i] = 4000 - 7*i;
					waveform3[i] = i*3;
      }

			wave = (struct waveform*) malloc(sizeof(struct waveform) + sizeof(int)*size );
			memcpy(&wave->point, waveform, sizeof(int)*size);
			wave->channel = 0;
			wave->size = size;
			
			printf("Sending wave\n");
			if ((res = ioctl(fd,DMMAT_ADD_WAVEFORM, wave)) < 0) perror(devname);
			
			wave2 = (struct waveform*) malloc(sizeof(struct waveform) + sizeof(int)*size );
			memcpy(&wave2->point, waveform2, sizeof(int)*size);
			wave2->channel = 1;
			wave2->size = size;
			
			printf("Sending wave2\n");
      //if ((res = ioctl(fd,DMMAT_ADD_WAVEFORM, wave2)) < 0) perror(devname);
			
			
			wave3 = (struct waveform*) malloc(sizeof(struct waveform) + sizeof(int)*size );
			memcpy(&wave3->point, waveform3, sizeof(int)*size);
			wave3->channel = 2;
			wave3->size = size;
			
			printf("Sending wave3\n");
      //if ((res = ioctl(fd,DMMAT_ADD_WAVEFORM, wave3)) < 0) perror(devname);
			
      //wave3->channel = 3;			
      //printf("Sending wave4\n");
      //if ((res = ioctl(fd,DMMAT_ADD_WAVEFORM, wave3)) < 0) perror(devname);
			
			
			if ((res = ioctl(fd,DMMAT_D2D_START)) < 0) perror(devname);
			
			
			free(wave);
			free(wave2);
			free(wave3);
			
      close(fd);

      return 0;
    }
	  case 'd':
	  {
	      printf("Running D2A IOCTL commands.\n");
	      const char* devname = "/dev/dmmat_d2a0";
	      int fd = open(devname,O_RDWR);

        /* Fill me in Scotty! The D2A IOCTL calls are
         *
         * DMMAT_D2A_GET_NOUTPUTS  
         * DMMAT_D2A_GET_CONVERSION   R[struct DMMAT_D2A_Conversion]
         * DMMAT_D2A_SET              W[struct DMMAT_D2A_Outputs]
         * DMMAT_D2A_GET              R[struct DMMAT_D2A_Outputs]
         * DMMAT_ADD_WAVEFORM         W[struct waveform] // D2A or D2D
         */
	      close(fd);

	      return 0;
	  }
    case 'a': 
    {
      printf("Running A2D IOCTL commands.\n");
      const char* devname = "/dev/dmmat_a2d0";
      int fd = open(devname,O_RDWR);
      // int res;
      // int i;
      
      int nchan;
      
      ioctl(fd, NIDAS_A2D_GET_NCHAN, &nchan);
      
      struct nidas_a2d_config cfg = { 20, 2};
        
      ioctl(fd, NIDAS_A2D_SET_CONFIG, &cfg);
      

      // struct nidas_a2d_sample_config scfg = {
      //  0,   // int sindex;         // sample index, 0,1,etc
      //  512, // int nvars;          // number of variables in sample
      //  20,  // int rate;           // sample rate
      //  NIDAS_FILTER_BOXCAR, // int filterType;     
      //                       // one of nidas_short_filter enum
      //  {1}, // int channels[MAX_A2D_CHANNELS];  
      //         // which channel for each variable
      //  {1}, // int gain[MAX_A2D_CHANNELS];     
      //         // gain setting for the channel
      //  {0}, // int bipolar[MAX_A2D_CHANNELS];// 1=bipolar,0=unipolar
      //  512, // int nFilterData;        // number of bytes in filterData;
      //  'd'  // char filterData[0];     // data for filter
      // };
      // 
      // 
      // ioctl(NIDAS_A2D_CONFIG_SAMPLE, scfg,
      //              sizeof(struct nidas_a2d_sample_config)+scfg.nFilterData);
      
      ioctl(fd, DMMAT_A2D_START);
      

      close(fd);
      
      return 0;
    }
		case 'e':
		{
			int res;
			printf("Stopping D2D.\n");
      const char* devname = "/dev/dmmat_d2d0";
      int fd = open(devname,O_RDWR);

			if ((res = ioctl(fd,DMMAT_D2D_STOP)) < 0) perror(devname);

      close(fd);

      return 0;
		}
      
  }

	return 0;
}





int main(int argc, char *argv[])
{
  int result;
  struct dmd_command send = {0, 0};
  
  if ( (result = check_options(argc, argv, &send)) > 0) return result;
  
  if (flags.total_set > 1){
    printf("Too many conflicting options set (r/w/s).\n");
    return 2;
  }
  
  switch (flags.mode){
    case 'r': 
      read_addr(&send);
      break;
    case 'w':
      write_addr(&send);
      break;
    case 's':
      start(flags.smode);
      break;
    default:
      break;
  }
  
  return 0;
}
