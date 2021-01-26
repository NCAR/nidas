//Passmark USB2 USB3 Linux example program
//usb_example.cpp
//Copyright PassMark Software 2010-2016
//www.passmark.com
//version 2.1.1002
//
//Requires the latest firmware for USB3 plugs (at least V2.0)

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <cmath>
#include <ctime>
#include "PassMarkUSB.h"

//Comment this out for use on older versions of libusb if you get compile errors for libusb_get_device_speed
//as the function was not available in all versions
#define USE_LIBUSB_FORSPEED 1

// These functions are overloaded to return a test result
int USB_2_BenchmarkTest(libusb_device_handle *handle_udev, int usbIndex);
int USB_3_BenchmarkTest(libusb_device_handle *handle_udev, int usbIndex);

//Example application to run a loopback and benchmark test
//Can use command line argument to pass the index (0 based integer) in usbinfo of plug to test or it will default to 0
int main(int argc, char *argv[])
{
	libusb_device_handle *handle_udev; //handle to USB plug after being connected
	unsigned long MaxTransferSize = 0;
	unsigned long CurrentTransferSize = 0;
	unsigned char *inBuffer = NULL;
	unsigned char *outBuffer = NULL;
	int plugindex = 0; //Plug under testing (index in usbInfo)
	int numUSBPLugs = 0;
        int test_result = 0; // 0=PASS, 1=FAIL
        int all_test_result = 0;

	//Process command line to get plug index
	if(argc == 2)
	{
		plugindex = atoi(argv[1]);
	}


	//Make sure USB list is empty
	memset(usbInfo, 0, sizeof(usbInfo));

	//Initialise USB library
	int ret = libusb_init(&USB_context);

	if(ret < 0)
	{
		printf("Failed to initialise libusb, error: %d\n", ret);
		return 1 ;
	}

	//Enable to see libusb debug messages
	//libusb_set_debug(USB_context, 4);

	//Get a list of all USB loopback devices on system
	numUSBPLugs = GetUSBPortsInfo();

	if(numUSBPLugs < 1)
	{
		printf("No USB loopback plugs found\n");
		return 1 ;
	}

	for(int count =0; count < numUSBPLugs; count++)
	{
		if(usbInfo[count].type == LOOPBACK_USB_2_PRODUCT_ID)
			printf("Found USB2 plug ");
		else if(usbInfo[count].type == LOOPBACK_USB_3_PRODUCT_ID)
			printf("Found USB3 plug : firmware: %0.1f ", usbInfo[count].fwVer );
		printf("%s speed: %d at %d:%d \n",  usbInfo[count].usbSerial, usbInfo[count].speed, usbInfo[count].bus, usbInfo[count].port);
	}

	//Check plug index is valid
	if(plugindex >= numUSBPLugs)
	{
		printf("ERROR: Loopback device not found at port index\n");
		return 1;
	}

	//Connect to selected plug
	if(ConnectUSBPlug(false, plugindex, USB_3_ST_SS, USB_3_TEST_LOOPBACK, &handle_udev,  &MaxTransferSize, &CurrentTransferSize,  &inBuffer, &outBuffer) == false)
	{
		printf("Couldn't connect to USB plug %d - %s\n", plugindex, usbInfo[plugindex].usbSerial);
		return 1;
	}

	if(usbInfo[plugindex].type == LOOPBACK_USB_3_PRODUCT_ID)
	{
		//Need to close and re-open device for USB 3 config
		if(ConnectUSBPlug ( true, plugindex,  USB_3_ST_SS, USB_3_TEST_LOOPBACK, &handle_udev, &MaxTransferSize, &CurrentTransferSize,  &inBuffer, &outBuffer) == false)
		{
			printf("Couldn't re-open USB3 plug %d - %s\n", plugindex, usbInfo[plugindex].usbSerial);
			return 1;
		}
	}


	//Short sleep before tests to display available plugs
	printf("Testing USB plug %s in 5 seconds\n", usbInfo[plugindex].usbSerial);
	sleep(5);

	//Run a loopback test
	printf("Loopback testing plug %s\n", usbInfo[plugindex].usbSerial);
	test_result = LoopbackTest(handle_udev, plugindex, MaxTransferSize, inBuffer, outBuffer);
        all_test_result |= test_result;

	//Run a benchmark test
	printf("Benchmarking plug %s \n", usbInfo[plugindex].usbSerial);

	if(usbInfo[plugindex].type == LOOPBACK_USB_3_PRODUCT_ID)
	{
		libusb_release_interface(handle_udev, 0);
		libusb_close(handle_udev);
		test_result = USB_3_BenchmarkTest(handle_udev, plugindex);
	}
	else
		test_result = USB_2_BenchmarkTest(handle_udev, plugindex);

	//Release device and close handle
	libusb_release_interface(handle_udev, 0);
	libusb_close(handle_udev);

	//Clear up buffers
	if (outBuffer)
		delete [] outBuffer;
	if (inBuffer)
		delete [] inBuffer;

	if (all_test_result == 0)
		printf("USB Test SUCCESS\n");
	else
		printf("USB Test FAILED, USB errors\n");

	return all_test_result;
}

//SendUSB_3_VendorCommand
//
// Send a Passmark specific USB3 vendor command
//	in:
//		USB3 device handle (where to send the command)
//
//
//
//	returned: Number of bytes returned from the device, 0 if error, -1 is ERROR_NOT_ENOUGH_MEMORY error

int SendUSB_3_VendorCommand (libusb_device_handle* udev, int wValue, unsigned char* buf, long bufflen )
{

	int dwBytesReturned = 0;
	int request = 0;


	if(wValue == USB_3_SET_CONFIG || wValue == USB_3_CONF_ERROR_COUNTERS || wValue == USB_3_RESET_ERROR_COUNTERS || (wValue & 0xFF) == USB_3_SET_DISP_MODE || (wValue & 0xFF) == USB_3_CONFIG_LPM_ENTRY)
		request = LIBUSB_REQUEST_TYPE_VENDOR ;
	else
		request = LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN;

	// If wValue is incorrect that this will timeout (-7)
	dwBytesReturned = libusb_control_transfer (
		udev,
		request,
		0,
		wValue,
		0,
		buf,
		bufflen,
		1000 );

	if(dwBytesReturned < 0)
	{
		printf("libusb_control_transfer failed %d\n", dwBytesReturned);
	}

	return ( ( int ) dwBytesReturned );
}

//SendUSB_2_VendorCommand
//
// Send a Passmark specific USB2 vendor command
//	in:
//		USB2 device handle (where to send the command)
//		The Requested mode
//		Parameter to be sent to USB device
//			- currently only used to indicate which LEDs to turn on/off
//  out:Pointer to the buffer of data returned from the device
//		For LOOPBACK || BENCHMARK || CHANGELEDS commands this is:
//			Size of buffer == 2
//			buffer[0] is response to change vendor command. Returns requested command if OK. Returns FF if NOK.
//			buffer[1] is T if device is high speed, F= if Fullspeed
//
//		For STATISTICS command this is:
//			Size of buffer == 0x32
//			buffer[0] is the number of bus errors and the interrupt limit for errors if OK. Returns FF if NOK.
//			buffer[1] is T if device is high speed, F= if Fullspeed
//			buffer[2] is the firmware version number (0 = version 1, 2 = version 2, 3 = version 3...)
//			buffer[3..n] is a PASSMARK copyright string
//
//	returned: Number of bytes returned from the device, 0 or <0 if error

int SendUSB_2_VendorCommand ( libusb_device_handle* udev,int ReqMode, int Parameter, unsigned char* bufferout, int bufferSize )
{
	int iNumOutParameters = USB_2_CHANGEMODEOUT;
	int bytesReturned = 0;
	int requestValue = 0;

	//Check a valid output buffer is passed
	if(bufferout == NULL)
	{
		return 0;
	}

	if (ReqMode == USB_2_STATISTICS)
	{
		//Check buffer is big enough
		if(bufferSize < USB_2_CHANGEMODEOUTSTAT)
			return 0;

		requestValue = USB_2_STATISTICS;
		iNumOutParameters = USB_2_CHANGEMODEOUTSTAT;
	}
	else if (ReqMode == USB_2_BENCHMARK)
	{
		requestValue = USB_2_BENCHMARK;
	}
	else if (ReqMode == USB_2_CHANGELEDS)
	{
		requestValue = Parameter*256+USB_2_CHANGELEDS;
	}
	else
	{
		requestValue = USB_2_LOOPBACK;
	}

	//Check buffer is big enough for the other modes (STATISTICS check previous)
	if(bufferSize < USB_2_CHANGEMODEOUT)
	{
		return 0;
	}

	//Clear buffer passed for output
	memset(bufferout, 0, bufferSize);

	//send control message
	bytesReturned = libusb_control_transfer(udev, LIBUSB_REQUEST_TYPE_VENDOR, USB_2_CHANGEMODE, requestValue, 0, bufferout, iNumOutParameters, 1000);
	return(bytesReturned);

}

//Name:	GetUSBDeviceInfo
//	Gets the serial number and the device description a specified  USB2/USB3 plug
//  The string descriptor is obtained using two separate calls.  The
//  first call is done to determine the size of the entire string descriptor,
//  and the second call is done with that total size specified.
//  For more information, please refer to the USB Specification, Chapter 9.
//Inputs
//  Udev - USB device
//	MaxSerialLen = Max length of buffer for serial num
//	MaxDescLen = Max length of buffer for description
//Outputs
//	Device serial number string
//	Device description string
//  Firmware version
//Returns
//	1 if OK
//	2 if OK, but EEPROM on device doesn't look to be programmed correctly
//	0 if error
int GetUSBDeviceInfo ( libusb_device *udev, char* serial, unsigned int serial_size, char* desc, unsigned int desc_size, float* fwVer)
{

	char buf1[128];
	char buf2[128];
	char buf3[128];
	int ret = 0;

	//Make sure buffers are large enough and pointers passed are usable
	if ( serial == NULL || desc == NULL || serial_size <= 16 || desc_size <= 64 || udev == NULL || fwVer == NULL)
		return 0;

	sprintf ( serial, "N/A" );
	sprintf ( desc, "N/A" );


	libusb_device_descriptor dev_descriptor;
	ret = libusb_get_device_descriptor ( udev, &dev_descriptor );
	if ( ret != 0 )
	{
		return 0;
	}

	if ( dev_descriptor.idProduct != LOOPBACK_USB_3_PRODUCT_ID && 
		dev_descriptor.idProduct != LOOPBACK_USB_2_PRODUCT_ID)
	{
		return 0;
	}


	libusb_device_handle* udev_handle;
	ret = libusb_open ( udev, &udev_handle );
	if ( ret != 0 )
	{
		return 0;
	}

	if ( libusb_get_string_descriptor_ascii ( udev_handle, dev_descriptor.iManufacturer, ( unsigned char* ) buf1, 64 ) < 0 )
	{
		libusb_close ( udev_handle );
		return 0;
	}


	if ( libusb_get_string_descriptor_ascii ( udev_handle, dev_descriptor.iProduct, ( unsigned char* ) buf2, 64 ) < 0 )
	{
		libusb_close ( udev_handle );
		return 0;
	}

	if ( libusb_get_string_descriptor_ascii ( udev_handle, dev_descriptor.iSerialNumber, ( unsigned char* ) buf3, 64 ) < 0 )
	{
		libusb_close ( udev_handle );
		return 0;
	}

	//If USB3 get firmware version
	if(dev_descriptor.idProduct == LOOPBACK_USB_3_PRODUCT_ID)
	{
		unsigned char devinfo[32];
		memset(devinfo, 0, sizeof(devinfo));
		ret = SendUSB_3_VendorCommand(udev_handle, USB_3_GET_DEVICE_INFO, (unsigned char*)devinfo, 31 );
		devinfo[31] = 0;
		char* tmpPtr = strstr((char*)devinfo, "FW Version V");
		if(tmpPtr != NULL)
		{
			*fwVer = (float)(tmpPtr[12] - '0') + ((float)(tmpPtr[14] - '0') /  10.0);
		}
	}

	libusb_close ( udev_handle );

	if ( strlen ( buf1 ) + strlen ( buf2 ) + 2 > desc_size || strlen ( buf3 ) +1 > serial_size )
		return 0;

	//For USB3 devices only use product discription
	if ( dev_descriptor.idProduct == LOOPBACK_USB_3_PRODUCT_ID )
		strcpy ( desc, buf2 );
	else
		sprintf ( desc, "%s %s", buf1, buf2 );

	strcpy ( serial, buf3 );

	if ( ( strncasecmp ( serial, "PM", 2 ) == 0 && strncasecmp ( desc, "PASS", 4 ) == 0 ) ||
		( strncasecmp ( serial, "TH", 2 ) == 0 ) )
		return 1;
	else
		return 2;

	return 0;
}

int GetUSBInfoFromlibusb()
{

	char	tmpSerial[MAXSERIALNUMLEN];
	int	numUSBOccurances = 0;
	char	USBSerial[50] = {'\0'};
	char	USBDesc[100] = {'\0'};
	int	productID = 0;
	float fmVer = 0;
	libusb_device **list;
	libusb_device **found = NULL;
	libusb_device_descriptor descriptor;

	if(USB_context == NULL)
	{
		printf ("USB_context is null - libusb failed to load\n");
		return 0;
	}

	int numdevices = libusb_get_device_list(USB_context, &list);

	if(numdevices < 0)
	{
		printf("Libusb error reading device list: %d\n", numdevices);
		return 0;
	}

	for(int count = 0; count < numdevices; count++)
	{
		libusb_device *device = list[count];
		if(libusb_get_device_descriptor(device, &descriptor) == 0)
		{
			GetUSBDeviceInfo ( device, USBSerial, 50, USBDesc, 100, &fmVer);

			if(descriptor.idVendor == LOOPBACK_VENDOR_ID)
			{
				if(descriptor.idProduct == LOOPBACK_USB_2_PRODUCT_ID ||
					descriptor.idProduct == LOOPBACK_USB_3_PRODUCT_ID )
				{
					fmVer = 0;

					if(GetUSBDeviceInfo ( device, USBSerial, 50, USBDesc, 100, &fmVer) > 0)
					{
						usbInfo[numUSBOccurances].port = 0;
						usbInfo[numUSBOccurances].bus = 0;
						usbInfo[numUSBOccurances].speed = 12;
						usbInfo[numUSBOccurances].type = descriptor.idProduct;
						usbInfo[numUSBOccurances].fwVer = fmVer;
						strcpy(usbInfo[numUSBOccurances].usbSerial, USBSerial);

#ifdef USE_LIBUSB_FORSPEED
						switch(libusb_get_device_speed(device))
						{
						case LIBUSB_SPEED_FULL:
							usbInfo[numUSBOccurances].speed = 12;
							break;

						case LIBUSB_SPEED_HIGH:
							usbInfo[numUSBOccurances].speed = 480;
							break;

						case LIBUSB_SPEED_SUPER :
							usbInfo[numUSBOccurances].speed = 5000;
							break;

						default:
							usbInfo[numUSBOccurances].speed = 12;
							break;
						};

#endif


						numUSBOccurances++;

					}
				}

			}
		}
	}

	libusb_free_device_list(list, 1);
	return numUSBOccurances;
}

//Enable low power entry for a USB3 plug
//Can only be used on USB3 plugs with firmware > v2.3
void EnableUSB3LowPowerEntry(libusb_device_handle *handle_udev)
{
		// CONFIG_LPM_ENTRY vendor command implemented in firmware version 2.3
		SendUSB_3_VendorCommand(handle_udev, USB_3_CONFIG_LPM_ENTRY | LPM_ENTRY_DISABLE,  0, 0);
		usleep(100000); // wait 100ms to allow the VBUS to staiblize and prevent 8b/10 errors
}

//Disable low power entry for a USB3 plug
//Disabling U1/U2 sleep modes entry during the test helps to prevent the surge currents needed when power cycling the USB3 PHY transceiver.
//Can only be used on USB3 plugs with firmware > v2.3
void DisableUSB3LowPowerEntry(libusb_device_handle *handle_udev)
{
		SendUSB_3_VendorCommand(handle_udev, USB_3_RESET_ERROR_COUNTERS, 0, 0);
}

//Enable low level errors count for a USB3 plug
//Can only be used on USB3 plugs with firmware > v2
void EnableUSB3ErrorCounts(libusb_device_handle *handle_udev)
{
		unsigned short errCfg = 0x1FF; //enable all type of errors
		SendUSB_3_VendorCommand(handle_udev, USB_3_CONF_ERROR_COUNTERS, (unsigned char*)&errCfg, 2);
		SendUSB_3_VendorCommand(handle_udev, USB_3_RESET_ERROR_COUNTERS, 0, 0);
}

//Get low level errors count from a USB3 plug
//Can only be used on USB3 plugs with firmware > v2
void GetUSB3GetErrorCounts(libusb_device_handle *handle_udev)
{

	unsigned char	errBuf[16] = { 0 };
	unsigned int  PhyErrCnt = 0;
	unsigned int  LnkErrCnt = 0;
	unsigned int LnkPhyErrorStatus;
	unsigned int PhyErrorStatus;
	int ret = 0;

	ret = SendUSB_3_VendorCommand(handle_udev, USB_3_GET_ERROR_COUNTS, errBuf, 16);
	if(ret > 0)
	{

		PhyErrCnt = *((unsigned int *)&errBuf[0]);
		LnkErrCnt = *((unsigned int *)&errBuf[4]);

		LnkPhyErrorStatus = *((unsigned int *)&errBuf[8]);
		PhyErrorStatus = *((unsigned int *)&errBuf[12]);

		//Physical Layer errors
		if(PhyErrCnt > 0 )
			printf("%u Physical Layer error(s) occurred.\n", PhyErrCnt);

		if (LnkPhyErrorStatus & USB_3_PHY_ERROR_EB_OVR_EV)
		{
			printf("Elastic Buffer Overflow error(s) occurred.\n");
		}
		if (LnkPhyErrorStatus & USB_3_PHY_ERROR_EB_UND_EV)
		{
			printf("Elastic Buffer Underflow error(s) occurred.\n");
		}
		if (LnkPhyErrorStatus & USB_3_PHY_ERROR_DISPARITY_EV)
		{
			printf("Receive Disparity error(s) occurred.\n");
		}
		if (LnkPhyErrorStatus & USB_3_RX_ERROR_CRC5_EV)
		{
			printf("Receive CRC-5 error(s) occurred.\n");
		}
		if (LnkPhyErrorStatus & USB_3_RX_ERROR_CRC16_EV)
		{
			printf("Receive CRC-16 error(s) occurred.\n");
		}
		if (LnkPhyErrorStatus & USB_3_RX_ERROR_CRC32_EV)
		{
			printf("Receive CRC-32 error(s) occurred.\n");
		}
		if (LnkPhyErrorStatus & USB_3_TRAINING_ERROR_EV)
		{
			printf("Training Sequence error(s) occurred.\n");
		}
		if (LnkPhyErrorStatus & USB_3_PHY_LOCK_EV)
		{
			printf("PHY Lock Loss error(s) occurred.\n");
		}

		//Link layer errors
		if(LnkErrCnt > 0)
			printf("%u Link Layer error(s) occurred.\n", LnkErrCnt);

		if (PhyErrorStatus & USB_3_HP_TIMEOUT_EN)
		{
			printf("HP_TIMEOUT error(s) occurred.\n");
		}
		if (PhyErrorStatus & USB_3_RX_SEQ_NUM_ERR_EN)
		{
			printf("RX_SEQ_NUM_ERR error(s) occurred.\n");
		}
		if (PhyErrorStatus & USB_3_RX_HP_FAIL_EN)
		{
			printf("RX_HP_FAIL error(s) occurred.\n");
		}
		if (PhyErrorStatus & USB_3_MISSING_LGOOD_EN)
		{
			printf("MISSING_LGOOD error(s) occurred.\n");
		}
		if (PhyErrorStatus & USB_3_MISSING_LCRD_EN)
		{
			printf("MISSING_LCRD error(s) occurred.\n");
		}
		if (PhyErrorStatus & USB_3_CREDIT_HP_TIMEOUT_EN)
		{
			printf("CREDIT_HP_TIMEOUT error(s) occurred.\n");
		}
		if (PhyErrorStatus & USB_3_PM_LC_TIMEOUT_EN)
		{
			printf("PM_LC_TIMEOUT error(s) occurred.\n");
		}
		if (PhyErrorStatus & USB_3_TX_SEQ_NUM_ERR_EN)
		{
			printf("TX_SEQ_NUM_ERR error(s) occurred.\n");
		}
		if (PhyErrorStatus & USB_3_HDR_ADV_TIMEOUT_EN)
		{
			printf("HDR_ADV_TIMEOUT error(s) occurred.\n");
		}
		if (PhyErrorStatus & USB_3_HDR_ADV_HP_EN)
		{
			printf("HDR_ADV_HP_EN error(s) occurred.\n");
		}
		if (PhyErrorStatus & USB_3_HDR_ADV_LCRD_EN)
		{
			printf("HDR_ADV_LCRD_EN error(s) occurred.\n");
		}
		if (PhyErrorStatus & USB_3_HDR_ADV_LGO_EN)
		{
			printf("HDR_ADV_LGO_EN error(s) occurred.\n");
		}

	}
	else
	{
		printf("USB3 get error count failed (%d)\n", ret);
	}
}


//Iterate through available USB plugs
//match to /proc/bus/usb/devices so we can get the Bus/Port number plug is currently on
//Returns number of USB2 and USB3 loopbacks found
int GetUSBPortsInfo()
{
	char *buf = NULL;
	char *tmpPtr = NULL;
	char *tmpUSBptr = NULL;
	char *tmpPosPtr = NULL;
	char *tmpPosPtr2 = NULL;
	char *tmpEndLinePtr = NULL;
	char tmpSerial[MAXSERIALNUMLEN];
	char tmpLoc[5];
	int count = 0 ;
	bool bsdStyle = false;

	FILE* fp = NULL;
	int size = 0;
	int numUSBOccurrences = 0;	//Total number of usb plug occurrences in devices file
	int len = 0;
	bool found = false;

	memset(usbInfo, 0, sizeof(usbInfo));

	//First get serial and speed from libusb
	numUSBOccurrences = GetUSBInfoFromlibusb();

	//Now try other system sources for bus/port info

	fp = fopen("/proc/bus/usb/devices", "r");
	if(fp == NULL)
	{
//		printf("/proc/bus/usb/devices not found - trying /sys\n");
		//Might not exist so try other location (eg Fedora 16 and newer)
		fp = fopen("/sys/kernel/debug/usb/devices", "r");
		if(fp == NULL)
		{
//			printf("/sys/kernel/debug/usb/devices not found - trying usbconfig\n");

			//Try BSD/MAC
			fp = (FILE*)popen("usbconfig dump_device_desc", "r");
			if(fp == NULL)
			{
				printf("usbconfig not found \n");
				return numUSBOccurrences;
			}
			else
				bsdStyle = true;
		}
	}

	if(bsdStyle == false)
	{
		//Count bytes in file, can't use fstat as it doesn't always work as expected
		while(!feof(fp))
		{
			if(fgetc(fp) == EOF)
				break;
			size++;
		}
		rewind(fp);

		buf = (char*)calloc(size+1, 1);

		if(buf == NULL)
		{
			fclose(fp);
			return 0;
		}

		//Search through the buffer for occurances of each USB plug
		int read = fread(buf, sizeof(char), size, fp);
		if(read > 0)
		{

			if(numUSBOccurrences > 0)
			{
				tmpPtr = buf;

				for(count = 0; count < numUSBOccurrences && count < MAXNUMUSBPORTS; count++)
				{
					memset(tmpSerial, 0, MAXSERIALNUMLEN);

					if(tmpPtr == NULL)
						break;

					//Find first Manufacturer=PASSMARK instance
					tmpPtr = strstr(tmpPtr, "Manufacturer=");
					while(tmpPtr != NULL)
					{
						if(strncasecmp(tmpPtr+13, "PASSMARK", 8) == 0)
							break;
						tmpPtr = strstr(tmpPtr+1, "Manufacturer=");
					}

					if(tmpPtr == NULL)
						break;

					//Find next serial number
					tmpUSBptr = strstr(tmpPtr, "SerialNumber=");

					if(tmpUSBptr == NULL)
						break;

					tmpEndLinePtr = strchr(tmpUSBptr, '\n');
					len = tmpEndLinePtr - tmpUSBptr - 13; //-13 to counter "SerialNumber: "

					//Check length is ok, otherwise could be another USB device and not passmark plug
					if(len > MAXSERIALNUMLEN)
					{
						buf = tmpPtr+1;
						continue;
					}

					//Save tmp serial
					memset(tmpLoc, 0, 5);
					strncpy(tmpSerial, tmpUSBptr+13, len);

					//Find matching plug from libusb info
					int currentPlug = 0;
					for(currentPlug = 0; currentPlug < numUSBOccurrences; currentPlug++)
					{
						if(strcmp(usbInfo[currentPlug].usbSerial, tmpSerial) == 0)
							break;
					}

					//couldn't find a match
					if(currentPlug == numUSBOccurrences)
					{
						tmpPtr = tmpPtr+1;
						continue;
					}

					//Need to search for Bus= until it is close to current pos
					tmpPosPtr = strstr(buf, "Bus=");

					while(tmpPosPtr != NULL && tmpPosPtr < tmpPtr)
					{
						tmpPosPtr2 = tmpPosPtr;
						tmpPosPtr = strstr(tmpPosPtr+1, "Bus=");
					}

					strncpy(tmpLoc, tmpPosPtr2+4, 2);
					usbInfo[currentPlug].bus = atoi(tmpLoc);

					tmpPosPtr = strstr(tmpPosPtr2, "Port=");
					strncpy(tmpLoc, tmpPosPtr+5, 2);
					usbInfo[currentPlug].port = atoi(tmpLoc) + 1;


					//Get speed
					tmpPosPtr = strstr(tmpPosPtr2, "Spd=");
					if(tmpPosPtr != NULL)
					{
						strncpy(tmpLoc, tmpPosPtr+4, 4);
						usbInfo[currentPlug].speed =  atoi(tmpLoc);
					}

					tmpPtr = tmpPtr+1;
					found = false;
				}
			}

		}

		fclose(fp);
		free(buf);
	}
	else
	{
		int infoCount = 0;
		count = 0;
		char tmpBuffer[1024];
		char* tmpPtr =NULL;
		int tmpBus = 0;
		int tmpPort = 0;
		int tmpSpeed = 0;
		char tmpSerial[MAXSERIALNUMLEN];

		while(fgets(tmpBuffer, 1024, fp) != NULL && count < MAXNUMUSBPORTS)
		{
			if(strncmp(tmpBuffer, "ugen", 3) != 0 || strcasestr(tmpBuffer, "PASSMARK") == NULL)
				continue;

			//Found Passmark USB device, read bus location and speed
			sscanf(tmpBuffer, "ugen%d.%d:%*[^(](%dMbps)", &tmpBus, &tmpPort, &tmpSpeed );
			//Read Device and serial number
			infoCount = 0;
			while(fgets(tmpBuffer, 1024, fp) != NULL && infoCount < 2)
			{
				if(strstr(tmpBuffer, "iSerialNumber") != NULL)
				{
					infoCount++;
					sscanf(tmpBuffer, "%*[^<]<%[^>]", tmpSerial);
				}
			}

			//Find matching plug from libusb info and save
			int currentPlug = 0;
			for(currentPlug = 0; currentPlug < numUSBOccurrences; currentPlug++)
			{
				if(strcmp(usbInfo[currentPlug].usbSerial, tmpSerial) == 0)
					break;
			}

			//couldn't find a match
			if(currentPlug == numUSBOccurrences)
				continue;

			usbInfo[currentPlug].bus = tmpBus;
			usbInfo[currentPlug].port = tmpPort;
			usbInfo[currentPlug].speed = tmpSpeed;

			count++;
		}
		fclose(fp);

	}

	return numUSBOccurrences;
}


//Name:	ConnectUSBPlug
// Open a handle to the matching USB plug in usbInfo[usbPlugIndex], sets it to loopback mode and
// gets plug ready for testing.
//Inputs
// bool bReconnect - set to true if reconnecting to a previoulsy opened plug (eg when opening a USB3 plug)
// usbPlugIndex - index of plug to test in usbInfo global
// USB3_SPEED Speed - Speed if conencting to a USB3 plug
// USB_3_TEST_MODE TestMode - Test mode if conencting to a USB3 plug (USB2 plugs will be placed in loopback mode)
//Outputs
// libusb_device_handle **handle_udev
// unsigned long *MaxTransferSize
// unsigned long *CurrentTransferSize
// unsigned char **inBuffer
// unsigned char **outBuffer
//Returns
//	true on sucess
//	false on failure

bool ConnectUSBPlug ( bool bReconnect, int usbPlugIndex, USB3_SPEED Speed, USB_3_TEST_MODE TestMode,  libusb_device_handle **handle_udev, unsigned long *MaxTransferSize, unsigned long *CurrentTransferSize , unsigned char **inBuffer, unsigned char **outBuffer)
{

	int	ret = 0;
	unsigned char	bufferout[USB_2_CHANGEMODEOUTSTAT] = {0};
	char	USBSerial[50] = {'\0'};
	char	USBDesc[100] = {'\0'};
	char  DeviceName[MAX_DRIVER_NAME] = "";
	float fmVer = 0;
	libusb_device **list;
	libusb_device_descriptor descriptor;
	libusb_device *device;
	bool found = false;


	if(USB_context == NULL)
	{
		printf("USB_context is null - libusb failed to load or hasn't been loaded\n");
		return false;
	}


	if ( bReconnect )
	{
		//Try closing and releasing handle though probably invalid now
		if ( *handle_udev != NULL )
		{
			//Don't close if release fails, can cause crash, especially in fedora 9
			int relRes = libusb_release_interface ( *handle_udev, 0 );
			if ( relRes == 0)
			{
				libusb_close ( *handle_udev );
			}
			else
			{
				*handle_udev = NULL;
			}

		}

		//Plug may still be renumerating so sleep before trying to access
		//extra long sleep for USB3 as on some systems (eg live boot form USB) it is a slow process
		if(usbInfo[usbPlugIndex].type == LOOPBACK_USB_3_PRODUCT_ID)
		{
			wait_USB ( 3000 );
		}
		else
			wait_USB ( 1500 );
	}

	int numdevices = libusb_get_device_list ( USB_context, &list );

	if ( numdevices < 1 )
	{
		if(numdevices == 0)
			printf("No USB devices returned\n");
		else //An error occured
			printf("Failed to get list of USb devices: %d\n", numdevices);

		return false;
	}

	for ( int count = 0; count < numdevices && found != true; count++ )
	{
		device = list[count];
		if ( libusb_get_device_descriptor ( device, &descriptor ) == 0 )
		{
			if ( descriptor.idVendor == LOOPBACK_VENDOR_ID )
			{
				if ( descriptor.idProduct == LOOPBACK_USB_2_PRODUCT_ID ||
					descriptor.idProduct == LOOPBACK_USB_3_PRODUCT_ID )
				{
					fmVer = 0;
					if ( GetUSBDeviceInfo ( device, USBSerial, 50, USBDesc, 100, &fmVer) > 0 )
					{
						if ( strcmp ( USBSerial, usbInfo[usbPlugIndex].usbSerial ) == 0 )
						{
							usbInfo[usbPlugIndex].type = descriptor.idProduct;
//							printf ( "Connecting [Serial: %s] \n", USBSerial );

							ret = libusb_open ( device, handle_udev );

							if ( ret != 0 )
							{
								//Couldn't connect
								printf("Could not open device, error: %d\n", ret);
								libusb_free_device_list ( list, 1 );
								return false;
							}

							int cur_config = 0;
							ret = libusb_get_configuration(*handle_udev, &cur_config);
							ret = libusb_set_configuration(*handle_udev, 1);

							found = true;
							break;
						}
					}
				}
			}
		}
	}

	if(found == false)
	{
		printf("Did not find Passmark USB plug %s\n", usbInfo[usbPlugIndex].usbSerial);
		libusb_free_device_list ( list, 1 );
		return false;
	}

	libusb_free_device_list ( list, 1 );

	//Before claim interface do a device reset to try and avoid issues where plug has been left in incomplete state
	//as seen on PCIE USB3 card in linux when using usb2 plugs
	if(usbInfo[usbPlugIndex].type == LOOPBACK_USB_2_PRODUCT_ID)
	{
		ret = libusb_reset_device( *handle_udev);
		if(ret != 0)
		{
			printf ("USB device reset failed: %s - err: %d\n", usbInfo[usbPlugIndex].usbSerial, ret);
		}
	}
	//Claim interface
	ret = libusb_claim_interface ( *handle_udev, 0 );
	if ( ret  != 0 )
	{
		//failed - try detaching and claiming again
		ret = libusb_detach_kernel_driver ( *handle_udev, 0 );
		ret = libusb_claim_interface ( *handle_udev, 0 );
		if ( ret != 0 )
		{
			//Could not claim device interface
			printf("Could not claim device interface, error: %d\n", ret);
			return false;
		}
	}


	//Clear halt state
	if(usbInfo[usbPlugIndex].type == LOOPBACK_USB_3_PRODUCT_ID)
	{
		ret = libusb_clear_halt ( *handle_udev,  USB_3_EPLOOPOUT );
		ret = libusb_clear_halt ( *handle_udev, USB_3_EPLOOPIN);
	}
	else
	{
		libusb_clear_halt ( *handle_udev, USB_2_EPLOOPOUT );
		libusb_clear_halt ( *handle_udev, USB_2_EPLOOPIN );
	}

	if(usbInfo[usbPlugIndex].type == LOOPBACK_USB_3_PRODUCT_ID )
	{
		//Need to configure USB3 plugs for loopback test
		if(bReconnect == false)
		{
			config_info_def usb3Config;
			memset(&usb3Config, 0, sizeof(config_info_def));

			if (TestMode == USB_3_TEST_LOOPBACK)
			{
				usb3Config.test_mode = USB_3_TEST_LOOPBACK;
				usb3Config.ep_type = USB_3_EP_BULK;
				usb3Config.ep_in = 1;
				usb3Config.ep_out = 1;
				usb3Config.ss_burst_len = 16;
				usb3Config.polling_interval = 1;
				usb3Config.hs_bulk_nak_interval = 0;
				usb3Config.iso_transactions_per_bus_interval = 1;
				usb3Config.iso_bytes_per_bus_interval = 0;
				usb3Config.speed = Speed;
				usb3Config.buffer_count = USB_3_LOOPBACK_BUFFER_COUNT;
				usb3Config.buffer_size = USB_3_LOOPBACK_BUFFER_SIZE;
			}
			else
			{
				if(TestMode == USB_3_TEST_BENCHMARK_RW) //OR RTW
					usb3Config.test_mode = 3;
				else if (TestMode == USB_3_TEST_BENCHMARK_WRITE)
					usb3Config.test_mode = 2;
				else
					usb3Config.test_mode = 1;

				usb3Config.ep_type = USB_3_EP_BULK;
				usb3Config.ep_in = 1;
				usb3Config.ep_out = 1;
				usb3Config.ss_burst_len = 12;
				usb3Config.polling_interval = 1;
				usb3Config.hs_bulk_nak_interval = 0;
				usb3Config.iso_bytes_per_bus_interval = 0;
				usb3Config.iso_transactions_per_bus_interval = 1;
				usb3Config.speed = Speed;


				if(TestMode == USB_3_TEST_BENCHMARK_RW)
				{
					usb3Config.buffer_count = USB_3_BENCHMARK_RW_BUFFER_COUNT;
					usb3Config.buffer_size = USB_3_BENCHMARK_RW_BUFFER_SIZE;
				}
				else
				{
					usb3Config.buffer_count = USB_3_BENCHMARK_BUFFER_COUNT;
					usb3Config.buffer_size = USB_3_BENCHMARK_BUFFER_SIZE;
				}
			}

			ret = SendUSB_3_VendorCommand(*handle_udev, USB_3_SET_CONFIG, (unsigned char*)&usb3Config, sizeof(usb3Config) );

			if ( ret <=0  )
			{
				printf("Failed to set USB3 plug to loopback mode (%d)\n", ret);
				return false;
			}
		}

	}
	else
	{
		//Set USB2 Firmware to Loopback Mode
		ret = SendUSB_2_VendorCommand (*handle_udev, USB_2_LOOPBACK, 0, bufferout, USB_2_CHANGEMODEOUTSTAT);

		if ( ret == 0xFF || ret <=0  )
		{
			printf("Failed to set USB2 plug to loopback mode (%d)\n", ret);
			return false;
		}

	}


	// initialise the transfer size and allocate the transfer buffers
	if ( usbInfo[usbPlugIndex].type  == LOOPBACK_USB_3_PRODUCT_ID )
	{
		if (TestMode == USB_3_TEST_LOOPBACK)
			*MaxTransferSize = USB_3_LOOPBACK_BUFFER_SIZE * USB_3_LOOPBACK_BUFFER_COUNT;
		else if(TestMode == USB_3_TEST_BENCHMARK_RW)
			*MaxTransferSize = USB_3_BENCHMARK_RW_BUFFER_SIZE * USB_3_BENCHMARK_RW_BUFFER_COUNT;
		else
			*MaxTransferSize = USB_3_BENCHMARK_BUFFER_SIZE * USB_3_BENCHMARK_BUFFER_COUNT;
		*CurrentTransferSize = *MaxTransferSize;
	}
	else
	{

		if(usbInfo[usbPlugIndex].speed == 12)
		{
			*MaxTransferSize = USB_2_LOOPBACKFSBUFFERSIZE;
		}
		else
		{
			*MaxTransferSize = USB_2_LOOPBACKHSBUFFERSIZE;
		}

		*CurrentTransferSize = *MaxTransferSize;
	}

	if(outBuffer != NULL && inBuffer != NULL)
	{
		if (*outBuffer)
			delete [] *outBuffer;
		if (*inBuffer)
			delete [] *inBuffer;

		*outBuffer = new unsigned char[*MaxTransferSize];
		*inBuffer = new unsigned char[*MaxTransferSize];

		if (*outBuffer == NULL || *inBuffer == NULL)
		{
			printf("Failed to allocate buffers\n");
			return false;
		}
	}

	return true;
}


//Break a sleep into smaller chunks, enables a check to be performed and break the sleep early if required
void wait_USB (int wait_time)
{
	while (wait_time > 0)
	{
		usleep(100000);
		wait_time -= 100;
		//Can add a check here for multi threading apps to break the wait if the overall tests are stopped
	}
}


//Run a Loopback test, stops after sending 1000 packets
int LoopbackTest(libusb_device_handle *handle_udev, int usbIndex, int maxTransferSize, unsigned char *inBuffer, unsigned char *outBuffer)
{
	bool error_USB = 0;
	bool packetIDMatch = false;
	int iBytesIncorrect = 0;
	int writeRes = 0;
	int readRes = 0;
	int numErrors = 0;
	int numIgnored = 0;
	unsigned long	currentTransferSize = maxTransferSize;
	unsigned long	i = 0;
	unsigned long	pattern = 1;
	unsigned long	testPass = 0;
	unsigned long	numPktsSent =0;
	unsigned long	numPktsRec = 0;
	unsigned long	totalBytesSent = 0;
	unsigned long	totalBytesRec = 0;
	int BytesWritten = 0 ;
	int BytesRead = 0 ;
	unsigned char buffercmd[USB_2_CHANGEMODEOUTSTAT] = {0};

	bool testRunning = true;
	DATA_PATTERN gDataPattern;

	if(usbInfo[usbIndex].type == LOOPBACK_USB_3_PRODUCT_ID && usbInfo[usbIndex].fwVer < 2.3f)
	{
		printf("Please update the firmware on your USB3 plug before running the loopback test\n");
		return 1;
	}

	if(usbInfo[usbIndex].type == LOOPBACK_USB_2_PRODUCT_ID)
	{
		//Clear bus error count (by reading the stats)
		SendUSB_2_VendorCommand(handle_udev, USB_2_STATISTICS,0, buffercmd, USB_2_CHANGEMODEOUTSTAT);

		//Turn off error LED (and Tx/Rx) 00 00 00 11 off off off on - Turn off all LEDS except Highspeed
		SendUSB_2_VendorCommand(handle_udev, USB_2_CHANGELEDS, 0x03, buffercmd, USB_2_CHANGEMODEOUTSTAT);
	}

	//Enable error counters USB3
	if(usbInfo[usbIndex].type == LOOPBACK_USB_3_PRODUCT_ID && usbInfo[usbIndex].fwVer >= 2.3f)
	{
		printf("Set USB3 error counter config\n");
		DisableUSB3LowPowerEntry(handle_udev);
		EnableUSB3ErrorCounts(handle_udev);
	}


	//Set data pattern for test
	gDataPattern = RANDOMBYTE;

	//Now enter the test loop - currently send/recv 1000 packets
	while(testRunning && testPass < 1000)
	{
		//Increment packet number
		testPass++;

		// initialize the buffer to send based on the pattern picked
		switch(gDataPattern)
		{
		case INCREMENTINGBYTE:
			for (i=0;i<currentTransferSize;i++)
				outBuffer[i] = (unsigned char) pattern++;
			break;
		case CONSTANTBYTE:
			for (i=0;i<currentTransferSize;i++)
				outBuffer[i] = (unsigned char) pattern;
			break;
		case RANDOMBYTE:
			for (i=0;i<currentTransferSize;i++)
				outBuffer[i] = rand();
			break;

		}

		//insert a packet number in the first WORD
		unsigned long * pBCurrent = (unsigned long *) &outBuffer[0];
		*pBCurrent = testPass;

		// initialize the in buffer
		memset(inBuffer, 0, maxTransferSize);

		//
		//Write packet and check it was sent
		//
		writeRes = 0;
		BytesWritten = 0;

		if(usbInfo[usbIndex].type == LOOPBACK_USB_3_PRODUCT_ID)
			writeRes = libusb_bulk_transfer ( handle_udev, USB_3_EPLOOPOUT, outBuffer, currentTransferSize, &BytesWritten, 1000 );
		else
			writeRes = libusb_bulk_transfer ( handle_udev, USB_2_EPLOOPOUT, outBuffer, currentTransferSize, &BytesWritten, 1000 );

		if (writeRes != 0)
		{
			error_USB = true;
			printf("Data packet %lu send failed - libusb_bulk_transfer failed %d : BytesWritten: %d\n",numPktsSent+1,  readRes, BytesWritten);
		}
		else
		{
			//bytesWritten = writeRes;
			totalBytesSent += BytesWritten;
			numPktsSent++;
		}

		if (BytesWritten != (int) currentTransferSize)
		{
			error_USB = true;
			printf("Data packet %lu send failed - %d bytes sent - expected %lu\n", numPktsSent, BytesWritten, currentTransferSize);	
			continue;
		}

		//Read packet and check it was the expected packet with the expected number of bytes
		packetIDMatch = false;
		numIgnored = 0;
		while(packetIDMatch == false && error_USB == false && numIgnored < 5)
		{

			readRes = 0;
			BytesRead = 0;

			if(usbInfo[usbIndex].type == LOOPBACK_USB_3_PRODUCT_ID)
				readRes = libusb_bulk_transfer ( handle_udev, USB_3_EPLOOPIN, inBuffer, currentTransferSize, &BytesRead,1000 );
			else
				readRes = libusb_bulk_transfer ( handle_udev, USB_2_EPLOOPIN, inBuffer, currentTransferSize, &BytesRead,1000 );

			if (readRes != 0)
			{
				printf("Data packet %lu receive failed - libusb_bulk_transfer failed %d\n", numPktsSent, readRes);
				error_USB = true;
				wait_USB(100);
			}
			else
			{
				//Update the number of bytes received
				totalBytesRec += BytesRead;
			}


			if (BytesRead != (int) currentTransferSize)
			{
				error_USB = true;
				printf("Data packet %lu receive failed - %d bytes read - expected %lu\n", numPktsSent, BytesRead, currentTransferSize);
				continue;
			}

			//Verify packet
			iBytesIncorrect = 0;
			for (i=0;i<currentTransferSize;i++)
			{
				if (inBuffer[i] != outBuffer[i])
					iBytesIncorrect++;
			}

			if (iBytesIncorrect > 0)
			{

				//Check packet number matches
				unsigned long * pulOut = (unsigned long *) &outBuffer[0];
				unsigned long * pulIn = (unsigned long *) &inBuffer[0];

				//If the data read is wrong, then maybe the last read failed and we are now reading data for the previous write.
				if(pulOut != pulIn)
				{
					if(numIgnored < 5)
					{
						printf ("Ignoring packet - ID mismatch\n");
					}
					else
					{
						printf ("Error - 5 packets in a row with an ID mismatch\n");
						error_USB = true;
					}
					numIgnored++;
				}
				else
				{
					error_USB = true;
					packetIDMatch = true;
					printf ("Incorrect byte in received packet %lu - byte %lu - expected %d - received %d\n\n", numPktsSent, i, outBuffer[i], inBuffer[i]);
				}

				continue;
			}
			else
			{
				packetIDMatch = true;
				//Increment received count
				numPktsRec++;
			}

			//Get USB3 low level error counts
			if(usbInfo[usbIndex].type == LOOPBACK_USB_3_PRODUCT_ID && usbInfo[usbIndex].fwVer >= 2.3f)
			{
				GetUSB3GetErrorCounts(handle_udev);
			}
		}

		if(error_USB)
		{
			numErrors++;
		}

//		if(testPass % 100 == 0)
//			printf ("Error Count: %d - Packets sent: %lu - Bytes sent: %lu - Bytes received %lu\n", numErrors, usbInfo[usbIndex].fwVer, numPktsSent, totalBytesSent, totalBytesRec);
	}

	if(usbInfo[usbIndex].type == LOOPBACK_USB_3_PRODUCT_ID && usbInfo[usbIndex].fwVer >= 2.3f)
		DisableUSB3LowPowerEntry(handle_udev);

        if (numErrors > 0) return 1;
	return 0;
}


//Run a benchmark test, stops after BENCHMARKCYCLES cycles
//void USB_2_BenchmarkTest(libusb_device_handle *handle_udev, int usbIndex)

//  Updated to return 0 for successful benchmark (BW > 100Mbit/s) or 1 for failure
int USB_2_BenchmarkTest(libusb_device_handle *handle_udev, int usbIndex)
{
	bool	ReadPhase = true;
	int	iRet = 0;
	int	error_USB = false;
	int	TestPass = 1;
	unsigned char	bufferout[USB_2_CHANGEMODEOUTSTAT] = {0};
	unsigned char*	HistBuffer = NULL;
	unsigned char*  outBuffer = NULL;
	unsigned char*  inBuffer = NULL;
	int		FIFOSize = 0;
	int	dwBytesWritten = 0, dwBytesRead = 0;
	int	writeRes = 0, readRes;
	int	CurrentHistTransferSize = USB_2_FSFIFOSIZE;
	int	iTotalHistory = 0, MaxHistory = 0, MinHistory = 0, NumFrames = 0, History[64] = {0};
	int	iTotalHistoryMeasured = 0, iFirstFrameVal = 0, iLastFrameVal = 0;
	float	flAveHistoryMeasured = 0.0, flAveHistoryPossible = 0.0;
	float	MaxRate = 0.0, MinRate = 0.0, AverageRate =0.0;
	float	OverallMaxRate =0.0, TotalOfAverageRate = 0.0, OverallAverageRate =0.0, OverallMinRate = 99999.0;
	int	iLastFrameIndex = 0;
	bool	TestInvalid = false;
	int	iNumTestInvalid = 0;
	float	g_flMaxReadRate = 0, g_flMaxWriteRate = 0, g_flMaxRate = 0;
	int benchmarkCycle = 0;
	bool g_bHighSpeed =  false;
	int MaxTransferSize = USB_2_BENCHMARKHSBUFFERSIZE;
	unsigned long long BytesSent= 0;
	unsigned long long BytesRec= 0;
	BENCHMARK_RESULTS BenchmarkResults;

	if(usbInfo[usbIndex].type == LOOPBACK_USB_3_PRODUCT_ID)
	{
		printf("Benchmark test not currently supported for USB3 under linux\n");
		return 1;
	}


	HistBuffer = new unsigned char[CurrentHistTransferSize];
	outBuffer = new unsigned char[USB_2_BENCHMARKHSBUFFERSIZE];
	inBuffer = new unsigned char[USB_2_BENCHMARKHSBUFFERSIZE];

	if(outBuffer == NULL || inBuffer == NULL ||  HistBuffer == NULL)
	{
		printf("Buffers have not been allocated\n");
		return 1;
	}

	if(usbInfo[usbIndex].speed == 480)
		g_bHighSpeed = true;

	memset(&BenchmarkResults, 0, sizeof(BenchmarkResults));
	memset(outBuffer, 0, sizeof(outBuffer));
	memset(inBuffer, 0, sizeof(inBuffer));

	for (int i=0;i<MaxTransferSize;i++ ) //No incrementing patterns for benchmark test, constant byte only
		outBuffer[i] = ( unsigned char ) 55;

	//Get statistics - clears errors etc
	iRet = SendUSB_2_VendorCommand (handle_udev, USB_2_STATISTICS,0, bufferout, USB_2_CHANGEMODEOUTSTAT);

//	printf("Benchmark: READING FROM USB DEVICE\n");

	//Run benchmark test for 10 cycles (5 x send 5 x receive)
	while(benchmarkCycle < 10 && error_USB == false)
	{
		//Need to send the vendor command every time
		//Put plug into benchmark Mode
		iRet = SendUSB_2_VendorCommand (handle_udev, USB_2_BENCHMARK, 0, bufferout, USB_2_CHANGEMODEOUTSTAT); //Set Firmware to Loopback Mode

		if ( iRet == 0xFF  || iRet <=0 )
		{
			//Plug might be disconnected
			printf("Unable to put plug into benchmark mode\n");
			error_USB = true;
			break;
		}

		//Must be a 10msec delay after sending a vendor command
		usleep(10000);

		//Read from plug
		if(ReadPhase)
		{
			dwBytesRead = 0;
			readRes = libusb_bulk_transfer ( handle_udev, USB_2_EPBENCHIN, inBuffer, MaxTransferSize, &dwBytesRead, 1000 );

			if(readRes < 0)
			{
				printf("Benchmark read failed: %d\n", readRes);
				error_USB = true;
				break;
			}

			if(dwBytesRead > 0 )
				BytesRec += dwBytesRead;
		}
		else
		{
			//Write to plug EPBENCHOUT
			dwBytesWritten = 0;
			writeRes = libusb_bulk_transfer ( handle_udev, USB_2_EPBENCHOUT, outBuffer, MaxTransferSize, &dwBytesWritten, 1000 );

			if(writeRes != 0)
			{
				printf("Benchmark write failed: %d\n", writeRes);
				error_USB = true;
				break;
			}

			if(dwBytesWritten > 0 )
				BytesSent += dwBytesWritten;

		}

		//Get a history report
		memset(HistBuffer, 0, CurrentHistTransferSize);
		//Sleep 70ms
		usleep(70000);
		dwBytesRead = 0;
		readRes = libusb_bulk_transfer ( handle_udev, USB_2_EPHISTORY, HistBuffer, CurrentHistTransferSize, &dwBytesRead, 1000 );

		// if the read of the History buffer failed, we want to stop the test
		if (readRes != 0)
		{
			printf("Benchmark Statistics Read failed (%d)\n", readRes);
			continue;
		}

		//Calculate the Maximum & Average speed from the history report
		MaxHistory = 0;
		MinHistory = 99999;//Dummy high value
		iTotalHistory = 0;
		iTotalHistoryMeasured = 0;
		flAveHistoryPossible = 0;
		iFirstFrameVal = 0;
		iLastFrameVal = 0;
		NumFrames = 0;
		iLastFrameIndex = 0;
		TestInvalid = false;
		for (int i=0;i<CurrentHistTransferSize;i++)
		{
			History[i] = (int) HistBuffer[i];
			if (History[i] != 0)
			{
				if (History[i] == 255)
				{
					printf("Benchmark: In Bulk NAK recieved - test invalid\n");
					TestInvalid = true;
				}
				else if (History[i] == 254)
				{
					printf("Benchmark: Ping NAK received  - test invalid\n");
					TestInvalid = true;
				}
				else 
				{
					if (History[i] != 0)
					{
						iLastFrameIndex = i;
						NumFrames++;
					}
				}
			}

			//Check for new maximum value
			if (History[i] > MaxHistory)
				MaxHistory = History[i];

			iTotalHistoryMeasured += History[i];
			if (History[i] != 0)
				iLastFrameVal = History[i];
		}

		if (NumFrames > 2)	//Remove ist and last microframes
		{
			for (int i=1;i<iLastFrameIndex;i++)
				if (History[i] > 0 && History[i] < MinHistory)	//Check for new minimum value, excluded 1st and last values
					MinHistory = History[i];
		}
		else				//Use all microframes
		{
			for (int i=0;i<=iLastFrameIndex;i++)
				if (History[i] > 0 && History[i] < MinHistory)
					MinHistory = History[i];
		}

		iFirstFrameVal = History[0];
		if (NumFrames == 0)
			TestInvalid = true;

		if (TestInvalid)
		{
			iNumTestInvalid++;

			if (iNumTestInvalid > 3)
			{
				iNumTestInvalid = 0;
			}
		}
		else
		{
			iNumTestInvalid = 0;

			if (NumFrames > 2)	//Remove 1st and last microframes
				flAveHistoryMeasured = (float)(iTotalHistoryMeasured - iFirstFrameVal - iLastFrameVal)/ (float)(NumFrames - 2);
			else				//Use all microframe data
				flAveHistoryMeasured = (float)iTotalHistoryMeasured/ (float)NumFrames;

			if (g_bHighSpeed)
			{
				if (ReadPhase == true)
					g_flMaxReadRate = fmaxf(g_flMaxReadRate, ((float) MaxHistory*512*8)/125);
				else
					g_flMaxWriteRate = fmaxf(g_flMaxWriteRate, ((float) MaxHistory*512*8)/125);

				MaxRate = ((float) MaxHistory*512*8)/125;
				AverageRate = ((float) flAveHistoryMeasured*512*8)/125;
				MinRate = ((float) MinHistory*512*8)/125;

			}
			else
			{
				//Fullspeed
				if (ReadPhase == true)
					g_flMaxReadRate = fmaxf(g_flMaxReadRate, ((float) MaxHistory*64*8)/1000);
				else
					g_flMaxWriteRate = fmaxf(g_flMaxWriteRate, ((float) MaxHistory*64*8)/1000);
				MaxRate = ((float) MaxHistory*64*8)/1000;
				AverageRate = ((float) flAveHistoryMeasured*64*8)/1000;
				MinRate = ((float) MinHistory*64*8)/1000;

			}


			if(ReadPhase == true)
			{
				if (MaxRate > BenchmarkResults.MaxReadSpeed)
					BenchmarkResults.MaxReadSpeed = MaxRate;
			}
			else
			{
				if (MaxRate > BenchmarkResults.MaxWriteSpeed)
					BenchmarkResults.MaxWriteSpeed = MaxRate;
			}

			if (MinRate < OverallMinRate && MinRate > 0)
				OverallMinRate = MinRate;

			//TotalOfAverageRate += (AverageRate + AverageRatePossible) / 2;
			TotalOfAverageRate += AverageRate ;

			// change to write operations to device after defined number of reads
			if (TestPass % READWRITECYCLES == 0)
			{
				//Start back at the orignial data block size
				if (g_bHighSpeed)
				{
					MaxTransferSize = USB_2_BENCHMARKHSBUFFERSIZE;
					FIFOSize = USB_2_HSFIFOSIZE;						// Only used for reporting
				}
				else
				{
					MaxTransferSize = USB_2_BENCHMARKFSBUFFERSIZE;
					FIFOSize = USB_2_FSFIFOSIZE;						// Only used for reporting
				}

				if (ReadPhase == false)
				{
					benchmarkCycle++;
					ReadPhase = true;
//					if(benchmarkCycle < BENCHMARKCYCLES)
//						printf("Benchmark: READING FROM USB DEVICE\n");

				}
				else
				{
					benchmarkCycle++;
					ReadPhase = false;
//					if(benchmarkCycle < BENCHMARKCYCLES)
//						printf("Benchmark: WRITING TO USB DEVICE\n");
				}
			}


			// update pass counter
			TestPass++;
			OverallAverageRate = TotalOfAverageRate / TestPass;
			g_flMaxRate = OverallMaxRate;

		}

	}

	delete [] outBuffer;
	delete [] inBuffer;

	if (OverallAverageRate < 100.0)
	{
		printf("FAILED Benchmark Test\nMax Read %0.0f Mbit/s\nMax Write %0.0f Mbit/s\nAverage Speed %0.0f Mbit/s\n", BenchmarkResults.MaxReadSpeed , BenchmarkResults.MaxWriteSpeed, OverallAverageRate);
		return 1; // Failed test
	}

	printf("Max Read %0.0f, Max Write %0.0f, Ave. %0.0f Mbit/s\n", BenchmarkResults.MaxReadSpeed , BenchmarkResults.MaxWriteSpeed, OverallAverageRate);

	return 0;
}


//Fill a passed buffer with a specified data pattern
void InitBuffer(unsigned long currentTransferSize, int dataPattern, unsigned char constPattern,  unsigned char *outBuffer)
{
	switch(dataPattern)
	{
	case INCREMENTINGBYTE:
		for (unsigned long i=0;i<currentTransferSize;i++)
			outBuffer[i] = (unsigned char) i;
		break;

	case CONSTANTBYTE:
		for (unsigned long i=0;i<currentTransferSize;i++)
			outBuffer[i] = (unsigned char) constPattern;
		break;

	case RANDOMBYTE:
		for (unsigned long i=0;i<currentTransferSize;i++)
			outBuffer[i] = (unsigned char) rand();
		break;

	}
}


//Name:	USB_3_BenchmarkLoop
// Run a benchmark test on the specified endpoint (in or out)
//Inputs
// libusb_device_handle *handle_udev - Open handle to a USB3 plug
// int usbIndex - index of plug to test in usbInfo global
// int endpoint - Endpoint to send/receive data from (USB_3_EPLOOPIN or USB_3_EPLOOPOUT)
// int NumXferPackets - Number of data packets to send
// long TransferSize - Size of data packets to send
void USB_3_BenchmarkLoop(libusb_device_handle *handle_udev, int usbIndex, int endpoint, int NumXferPackets, long TransferSize)
{

	int			BytesXferred = 0;
	unsigned long long	TotalBytesXferred = 0;
	bool			bBenchmarkStatus = false;
	long			lBenchmarkBytes = 0;
	int			WaitResult;
	int			xferCount = 0;
	double	dRate;
	double			dMaxRate = 0.0f;
	timespec		timerStart, timerEnd;

	memset(&timerStart, 0, sizeof(timerStart));
	memset(&timerEnd, 0, sizeof(timerEnd));

	// Allocate the data buffer
	unsigned char	 *dataBuffer = new unsigned char[TransferSize];

	long len = TransferSize;

	//Init data buffer
	InitBuffer(TransferSize, RANDOMBYTE, 0, dataBuffer);

	//Get start time of transfer
	clock_gettime(CLOCK_MONOTONIC, &timerStart);

	// xfer loop.
	while (xferCount < NumXferPackets)
	{

		BytesXferred = 0;
		int readRes = libusb_bulk_transfer ( handle_udev, endpoint, dataBuffer, TransferSize, &BytesXferred, 1000 );

		if(readRes < 0 || BytesXferred < TransferSize)
		{
			printf("Benchmark transfer failed: %d\n", readRes);
			break;
		}

		TotalBytesXferred += BytesXferred;
		xferCount++;

		//Every 64 transfers update the speed
		if (xferCount % 64 == 0 || xferCount == NumXferPackets)
		{
			clock_gettime(CLOCK_MONOTONIC, &timerEnd);

			double timeTaken = 0;
			timeTaken = (double)(( timerEnd.tv_sec - timerStart.tv_sec) * 1000 ) + ((double)(timerEnd.tv_nsec - timerStart.tv_nsec) / (double)1000000); //in miliseconds
			if(timeTaken > 0)
				dRate = (double)(TotalBytesXferred * 8) / timeTaken / (double)1000; //Megabits / second
			else
				dRate = 0;

			if (dRate > dMaxRate)
				dMaxRate = dRate;

			printf("Transfer count %d/%d - Transfer rate: %0.2f Mbit/s (Max rate: %0.2f Mbit/s)\n", xferCount, NumXferPackets, dRate, dMaxRate);

			TotalBytesXferred = 0;
			clock_gettime(CLOCK_MONOTONIC, &timerStart);
		}
	} 

	if(dataBuffer != NULL)
		delete [] dataBuffer;

}


//Run a benchmark test (write then read) on the selected USB3 plug
//void USB_3_BenchmarkTest(libusb_device_handle *handle_udev, int usbIndex)

//  Updated to return 0 for successful benchmark (BW > 100Mbit/s) or 1 for failure
//  As of 12/2020, this function not fully supported for return value
int USB_3_BenchmarkTest(libusb_device_handle *handle_udev, int usbIndex)
{

	if (usbInfo[usbIndex].fwVer >= 2)
	{

		unsigned long CurrentTransferSize = 2097152; //2MB
		unsigned long MaxTransferSize = CurrentTransferSize;

		if(ConnectUSBPlug(false, usbIndex, USB_3_ST_SS, USB_3_TEST_BENCHMARK_RW, &handle_udev,  &MaxTransferSize, &CurrentTransferSize, NULL, NULL) == false)
		{
			printf("Couldn't connect to USB plug %d - %s\n", usbIndex, usbInfo[usbIndex].usbSerial);
			return 1;
		}

		//Need to close and re-open device for USB 3 config change
		if(ConnectUSBPlug (true, usbIndex, USB_3_ST_SS, USB_3_TEST_BENCHMARK_RW, &handle_udev, &MaxTransferSize, &CurrentTransferSize, NULL, NULL) == false)
		{
			printf("Couldn't re-open USB3 plug %d - %s\n", usbIndex, usbInfo[usbIndex].usbSerial);
			return 1;
		}

		printf("Turn LCD display off\n");
		SendUSB_3_VendorCommand(handle_udev, USB_3_SET_DISP_MODE | USB_3_DISPLAY_DISABLE,  0, 0);
		printf("Read benchmark\n");
		USB_3_BenchmarkLoop(handle_udev,  usbIndex, USB_3_EPLOOPIN, 1024, CurrentTransferSize);

		//LCD can turn back on automatically if there is a break in benchmark data being sent
		printf("Turn LCD display off\n");
		SendUSB_3_VendorCommand(handle_udev, USB_3_SET_DISP_MODE | USB_3_DISPLAY_DISABLE,  0, 0);
		printf("Write benchmark\n");
		USB_3_BenchmarkLoop(handle_udev, usbIndex, USB_3_EPLOOPOUT, 1024, CurrentTransferSize);

		printf("Turn LCD display on\n");
		SendUSB_3_VendorCommand(handle_udev, USB_3_SET_DISP_MODE + USB_3_DISPLAY_ENABLE,  0, 0);

	}
	else
	{
		printf("Please update the firmware on your USB3 plug before running the benchmark test\n");
	}

	return 1; // Return Error, this function does not fully support return value
}
