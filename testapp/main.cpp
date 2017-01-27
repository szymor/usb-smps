/*
 * main.cpp
 *
 *  Created on: 3 sie 2014
 *  USB power supply control CLI application
 */

#include <cstdio>
#include <cstdlib>
#include <stdint.h>
#include <cstring>
#include <hidapi/hidapi.h>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#define USBDEV_SHARED_VENDOR	0x16c0
#define USBDEV_SHARED_PRODUCT	0x05df

using namespace std;

struct __attribute__((aligned(8))) SUSBPacket
{
	uint8_t report_id;
	uint8_t output_state;
	uint16_t voltage_value;
	uint16_t amperage_value;
	uint8_t res1, res2, res3;
};

struct SPair
{
	double voltage;
	double current;
	unsigned int time;
	unsigned int outstate;
};

hid_device *device = NULL;

void help(char* name);
void delay(unsigned int seconds);
void actionSetValuesFromList(char* csvpath);
void actionSetValue(double voltage, double current, bool outstate);
void actionGetValue(void);

int main(int argc, char** argv)
{
	if(argc == 1)
	{
		help(argv[0]);
		return 0;
	}

	hid_init();
	device = hid_open(USBDEV_SHARED_VENDOR, USBDEV_SHARED_PRODUCT, NULL);
	if(!device)
	{
		printf("No device with vid:pid=%04x:%04x could be found.\n", USBDEV_SHARED_VENDOR, USBDEV_SHARED_PRODUCT);
		exit(3);
	}

	if(!strcmp(argv[1],"-l"))
	{
		actionSetValuesFromList(argv[2]);
	}
	else if(!strcmp(argv[1],"-s"))
	{
		double voltage, current;
		bool outstate;
		voltage = atof(argv[2]);
		current = atof(argv[3]);
		outstate = atoi(argv[4]) ? 1 : 0;
		actionSetValue(voltage,current,outstate);
	}
	else if(!strcmp(argv[1],"-g"))
	{
		actionGetValue();
		printf("\n");
	}
	else if(!strcmp(argv[1],"-gc"))
	{
		while(1)
		{
			actionGetValue();
			printf("\r");
			fflush(stdout); // printf flushes stdout buffer usually only at \n
			delay(1);
		}
	}
	else
	{
		help(argv[0]);
		return 0;
	}

	hid_exit();
	return 0;
}

void help(char* name)
{
	printf("Usage:\n");
	printf(" %s -l valuelist.csv\n\tcsv row format: voltage[V],current[A],delay[sec]\n",name);
	printf(" %s -s voltage_in_V current_in_A outstate_as_0_or_1\n",name);
	printf(" %s -g\n",name);
	printf(" %s -gc\n",name);
	exit(0);
}

void delay(unsigned int seconds)
{
#ifdef _WIN32
	Sleep(seconds*1000);
#else
	sleep(seconds);
#endif
}

void actionSetValuesFromList(char* csvpath)
{
	vector<SPair> pairs;
	SPair pair;
	FILE* file;

	file = fopen(csvpath,"r");
	if(!file)
	{
		printf("File read error.\n");
		exit(1);
	}
	while(feof(file) == 0)
	{
		if(fscanf(file,"%lf,%lf,%d,%d",&pair.voltage,&pair.current,&pair.time,&pair.outstate) == 4)
			pairs.push_back(pair);
		else
			break;
	}
	fclose(file);

	if(pairs.size() == 0)
	{
		printf("No pairs have been read.\n");
		exit(2);
	}

	printf("[V]\t[A]\t[s]\toutput[bool]\n");
	for(unsigned int i = 0; i < pairs.size(); ++i)
	{
		printf("%.2lf\t%.2lf\t%d\t%d\n",pairs[i].voltage,pairs[i].current,pairs[i].time,pairs[i].outstate);
	}
	printf("\n");

	for(unsigned int i = 0 ;; ++i)
	{
		if(i == pairs.size())
			i = 0;
		actionSetValue(pairs[i].voltage,pairs[i].current,pairs[i].outstate);
		for(unsigned int j = 0; j < pairs[i].time; ++j)
		{
			actionGetValue();
			printf("\r");
			fflush(stdout);
			delay(1);
		}
	}

}

void actionSetValue(double voltage, double current, bool outstate)
{
	SUSBPacket packet;
	uint32_t temp;
	packet.report_id = 0;
	packet.output_state = outstate ? 1 : 0; // always on
	temp = voltage*65536/36;
	if(temp > 0xffff)
		temp = 0xffff;
	packet.voltage_value = (uint16_t)temp;
	temp = current*65536/2;
	if(temp > 0xffff)
		temp = 0xffff;
	packet.amperage_value = (uint16_t)temp;
	//printf("%05x %05x\n",packet.voltage_value,packet.amperage_value);
	if(hid_send_feature_report(device,(unsigned char*)&packet,sizeof(packet)) == -1)
	{
		printf("Error writing to USB device.\n");
		exit(5);
	}
}

void actionGetValue(void)
{
	SUSBPacket packet;
	packet.report_id = 0;
	if(hid_get_feature_report(device,(unsigned char*)&packet,sizeof(packet)) == -1)
	{
		printf("Error reading from USB device.\n");
		exit(4);
	}
	printf("Output state: %3s\t",packet.output_state ? "ON" : "OFF");
	printf("Voltage: %lf\tCurrent: %lf", ((double)packet.voltage_value)*90/65536, ((double)packet.amperage_value)*3.333/65536);
}
