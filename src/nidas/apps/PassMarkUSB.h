//Passmark USB2 USB3 Linux example program
//PassMarkUSB.h
//Copyright PassMark Software 2010-2017
//www.passmark.com
//version 2.1.1001
//
//Requires the latest firmware for USB3 plugs (at least V2.3)

#ifndef PASSMARK_USB_H
#define PASSMARK_USB_H

//Include Linux USB library
#include <libusb-1.0/libusb.h>

#define BUF_256 256
#define BUF_128 128

//Vendor and product IDs for PassMark USB plugs
#define LOOPBACK_VENDOR_ID 0x0403
#define LOOPBACK_USB_2_PRODUCT_ID 0xff0a
#define LOOPBACK_USB_3_PRODUCT_ID 0xff0b

#define MAXNUMUSBPORTS 20
#define MAXSERIALNUMLEN 20
#define MAX_DRIVER_NAME 64

//
// USB2 Firmware definitions
//
// Firmware vendor command to change modes
#define USB_2_CHANGEMODE		0xB0	// Passmark vendor command setup in firmware
#define	USB_2_CHANGEMODEIN	1		// Number of parameters sent to this firmware command
#define USB_2_CHANGEMODEOUT	2

#define USB_2_CHANGEMODEOUTSTAT 50
#define USB_2_INITIALISATION 0
#define USB_2_LOOPBACK 1
#define USB_2_BENCHMARK 2
#define USB_2_STATISTICS 3
#define USB_2_CHANGELEDS 4

#define USB_2_EPLOOPOUT 0x02 //Loopback out endpoint
#define USB_2_EPLOOPIN 0x86 //Loopback in endpoint

#define USB_2_EPBENCHIN 0x88  //Benchmark in endpoint
#define USB_2_EPBENCHOUT 0x04 //Benchmark out endpoint
#define USB_2_EPHISTORY 0x81  //history report endpoint

//
// Host definitions
//
#define  USB_2_FSFIFOSIZE 64				// Size of blocks handled by USB device (chuncked into these blocks at a lower layer)
#define  USB_2_HSFIFOSIZE 512				// Size of blocks handled by USB device (chuncked into these blocks at a lower layer)
#define  USB_2_LOOPBACKFSBUFFERSIZE 64		// Buffersize for loopback in FullSpeed mode = FIFO buffer size
#define  USB_2_LOOPBACKHSBUFFERSIZE 512	// Buffersize for loopback in HighSpeed mode = FIFO buffer size - 4095 is linux limit?

#define  USB_2_BENCHMARKFSBUFFERSIZE 2048	// Buffersize for benchmark in FullSpeed mode = worst case history buffer full
#define  USB_2_BENCHMARKHSBUFFERSIZE 32768	// Buffersize for benchmark in HighSpeed mode = worst case history buffer full
#define  USB_2_BENCHMARKHISTBUFFERSIZE 64  // Buffersize for benchmark History report

#define READWRITECYCLES 10		//Number of times bencmark test will send or read data during a cycle
#define BENCHMARKCYCLES 10		//Number of times benchmark will run read/write cycle


//USB3 definitions
#define	USB_3_EPLOOPOUT 0x01
#define	USB_3_EPLOOPIN 0x81

//USB3 Plug benchmakr read then write or read+write buffer size
#define	USB_3_BENCHMARK_RW_BUFFER_SIZE	24576
#define	USB_3_BENCHMARK_RW_BUFFER_COUNT	2
//USB3 plug benchmark read or write only buffer size
#define USB_3_BENCHMARK_BUFFER_SIZE 49512
#define USB_3_BENCHMARK_BUFFER_COUNT 2
//USB3 plug loopback buffer size
#define	USB_3_LOOPBACK_BUFFER_SIZE	1024
#define	USB_3_LOOPBACK_BUFFER_COUNT	64

typedef enum _USB_3_TEST_MODE
{
	USB_3_TEST_LOOPBACK = 0,
	USB_3_TEST_BENCHMARK_READ,
	USB_3_TEST_BENCHMARK_WRITE,
	USB_3_TEST_BENCHMARK_RW
} USB_3_TEST_MODE;

typedef enum _USB_3_EP_TYPE
{
	USB_3_EP_CONTROL = 0,
	USB_3_EP_ISOCHRONOUS,
	USB_3_EP_BULK,
	USB_3_EP_INTERRUPT
} USB_3_EP_TYPE;

#pragma pack(push, 1)
typedef struct 
{
	unsigned char test_mode;
	unsigned char ep_type;
	unsigned char ep_in;
	unsigned char ep_out;
	unsigned char ss_burst_len;
	unsigned char polling_interval;
	unsigned char hs_bulk_nak_interval;
	unsigned char iso_transactions_per_bus_interval;
	unsigned short iso_bytes_per_bus_interval;
	// settings added from firmware version 2.0
	unsigned char speed;
	unsigned char buffer_count;
	unsigned short buffer_size;
} config_info_def;
#pragma pack(pop)

enum 
{
	USB_3_CHANGELEDS = 1,
	USB_3_SET_CONFIG,
	USB_3_GET_CONFIG,
	USB_3_SET_DISP_MODE,
	USB_3_CONF_ERROR_COUNTERS,
	USB_3_GET_ERROR_COUNTS,
	USB_3_GET_VOLTAGE,
	USB_3_RESERVED_DONOTUSE = 0x08,
	USB_3_GET_MAX_SPEED, 
	USB_3_RESET_ERROR_COUNTERS,
	USB_3_CONFIG_LPM_ENTRY,
	USB_3_GET_DEVICE_INFO = 0x50
};

typedef enum _USB_3_SPEED
{
	USB_3_ST_UNKNOWN = 0, 
	USB_3_ST_FS, 
	USB_3_ST_HS,
	USB_3_ST_SS,

} USB3_SPEED;

#define USB_3_DISPLAY_DISABLE	 0
#define USB_3_DISPLAY_ENABLE	 (1 << 8)

#define LPM_ENTRY_DISABLE	 0
#define LPM_ENTRY_ENABLE	 (1 << 8)

#define	USB_3_PHY_ERROR_DECODE_EV		(1 << 0)
#define	USB_3_PHY_ERROR_EB_OVR_EV		(1 << 1)
#define	USB_3_PHY_ERROR_EB_UND_EV		(1 << 2)
#define	USB_3_PHY_ERROR_DISPARITY_EV		(1 << 3)
#define	USB_3_RX_ERROR_CRC5_EV			(1 << 5)
#define	USB_3_RX_ERROR_CRC16_EV			(1 << 6)
#define USB_3_RX_ERROR_CRC32_EV			(1 << 7)
#define	USB_3_TRAINING_ERROR_EV			(1 << 8)
#define	USB_3_PHY_LOCK_EV			(1 << 9)

#define	USB_3_HP_TIMEOUT_EN			(1 << 0)
#define	USB_3_RX_SEQ_NUM_ERR_EN			(1 << 1)
#define	USB_3_RX_HP_FAIL_EN			(1 << 2)
#define	USB_3_MISSING_LGOOD_EN			(1 << 3)
#define	USB_3_MISSING_LCRD_EN			(1 << 4)
#define	USB_3_CREDIT_HP_TIMEOUT_EN		(1 << 5)
#define	USB_3_PM_LC_TIMEOUT_EN			(1 << 6)
#define	USB_3_TX_SEQ_NUM_ERR_EN			(1 << 7)
#define	USB_3_HDR_ADV_TIMEOUT_EN		(1 << 8)
#define	USB_3_HDR_ADV_HP_EN			(1 << 9)
#define	USB_3_HDR_ADV_LCRD_EN			(1 << 10)
#define	USB_3_HDR_ADV_LGO_EN			(1 << 11)

//Data Structures
typedef struct USBInfo
{
	char usbSerial[MAXSERIALNUMLEN];
	int bus;
	int port;
	int type;	//USB2 or 3, check against LOOPBACK_USB2_PRODUCT_ID and LOOPBACK_USB3_PRODUCT_ID
	int speed;
	float fwVer;	//From FW V2.0 can be read direct from USB3 plug
} *USBInfoPtr;

typedef enum _DATA_PATTERN {
	INCREMENTINGBYTE,
	RANDOMBYTE,
	CONSTANTBYTE
} DATA_PATTERN;

typedef struct _BENCHMARK_RESULTS
{
	float MaxReadSpeed;
	float MaxWriteSpeed;
} BENCHMARK_RESULTS;

//Globals
libusb_context* USB_context;
USBInfo	usbInfo[MAXNUMUSBPORTS+1];

//Function defs
int GetUSBPortsInfo();
int SendUSB_3_VendorCommand (libusb_device_handle* udev, int wValue, unsigned char* buf, long bufflen );
int SendUSB_2_VendorCommand ( libusb_device_handle* udev,int ReqMode, int Parameter, unsigned char* bufferout, int bufferSize );
bool ConnectUSBPlug ( bool bReconnect, int usbPlugIndex, USB3_SPEED Speed, USB_3_TEST_MODE TestMode,  libusb_device_handle **handle_udev, unsigned long *MaxTransferSize, unsigned long *CurrentTransferSize , unsigned char **inBuffer, unsigned char **outBuffer);
int GetUSBDeviceInfo ( libusb_device_handle *udev, char* serial, unsigned int serial_size, char* desc, unsigned int desc_size, float* fwVer);
void wait_USB (int wait_time) ;

// Removed from Header because defined in main file and prototype mush change
//void USB_2_BenchmarkTest(libusb_device_handle *handle_udev, int usbIndex);
//void USB_3_BenchmarkTest(libusb_device_handle *handle_udev, int usbIndex);

int LoopbackTest(libusb_device_handle *handle_udev, int usbIndex, int maxTransferSize, unsigned char *inBuffer, unsigned char *outBuffer);

#endif //PASSMARK_USB_H
