/* utp_com tool is used to send commands to hardware via Freescale's UTP protocol.
 * Copyright (C) 2015 Ixonos Plc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <scsi/sg_lib.h>
#include <scsi/sg_io_linux.h>

#define BUSY_SLEEP           500000
#define BUSY_CHECK_COUNT     500
#define CMD_TIMEOUT          (5 * 60 * 1000)
#define UTP_CMD_SIZE         0x10
#define UTP_REPLY_BYTE       13
#define SENSE_BUF_SIZE       16
#define FILE_SIZE_OFFSET     10
#define MAX_SENT_DATA_SIZE   0x10000

// UTP reply codes
#define UTP_REPLY_PASS 0
#define UTP_REPLY_EXIT 1
#define UTP_REPLY_BUSY 2
#define UTP_REPLY_SIZE 3

int extra_info = 0;

struct __attribute__ (( packed )) utp_cmd
{
	uint8_t data[UTP_CMD_SIZE];
};

struct utp_cmd poll =
{
	.data = {0xf0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}
};

struct utp_cmd exec =
{
	.data = {0xf0, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}
};

struct utp_cmd put =
{
	.data = {0xf0, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}
};

void print_help()
{
	printf("Usage: 'upt_com -d device -c command [-f file] [-e]'\n");
	printf("\t-d\tdevice\n");
	printf("\t-c\tcommand to be run\n");
	printf("\t-f\tfile to be set\n");
	printf("\t-e\textra info prints\n");
	printf("\n\te.g. sudo ./upt_com -d /dev/sdb -c \"$ uname -r\"\n");
}

int send_cmd(int device_fd, struct utp_cmd *cmd, void *dxferp, int dxferp_len, uint8_t *utp_reply_code)
{
	int ret;
	uint8_t sensebuf[SENSE_BUF_SIZE] = {0};

	// Create sg io header
	struct sg_io_hdr sgio_hdr;
	memset(&sgio_hdr, 0, sizeof(sgio_hdr));
	sgio_hdr.sbp = sensebuf;
	sgio_hdr.interface_id = 'S';
	sgio_hdr.timeout = CMD_TIMEOUT;
	sgio_hdr.cmd_len = UTP_CMD_SIZE;
	sgio_hdr.cmdp = (unsigned char *)cmd->data;
	sgio_hdr.mx_sb_len = SENSE_BUF_SIZE;
	sgio_hdr.dxfer_direction = SG_DXFER_TO_DEV;
	sgio_hdr.dxfer_len = dxferp_len;
	sgio_hdr.dxferp = dxferp;

	// Print CDB data
	if (extra_info)
	{
		int i;
		for (i = 0; i < UTP_CMD_SIZE; i++)
		{
			printf("Sent data %02d: 0x%02x\n", i, sgio_hdr.cmdp[i]);
		}
	}

	// Call IOCTL
	ret = ioctl(device_fd, SG_IO, &sgio_hdr);
	if (ret < 0)
	{
		fprintf(stderr, "SG_IO ioctl error\n");
		close(device_fd);
		return 1;
	}

	// Print sense data
	if (extra_info)
	{
		int i;
		for (i = 0; i < SENSE_BUF_SIZE; i++)
		{
			uint8_t sense_data = sensebuf[i];

			if (sense_data != 0)
			{
				printf("Sense data %02d: 0x%02x\n", i, sense_data);
			}
		}
	}

	if (utp_reply_code)
	{
		*utp_reply_code = sensebuf[UTP_REPLY_BYTE];
	}

	if (sensebuf[UTP_REPLY_BYTE] == UTP_REPLY_EXIT)
	{
		fprintf(stderr, "UTP_REPLY_EXIT\n");
		close(device_fd);
		return 1;
	}

	return 0;
}

int main(int argc, char * argv[])
{
	int c;
	int ret;
	int file_fd;
	int device_fd;
	struct stat st;
	char *command = NULL;
	char *file_name = NULL;
	char *file_data = NULL;
	char *device_name = NULL;

	opterr = 0;

	// Parse parameters
	while ((c = getopt(argc, argv, "c:d:ef:")) != -1)
	{
		switch (c)
		{
			case 'c':
				command = optarg;
				break;
			case 'd':
				device_name = optarg;
				break;
			case 'e':
				extra_info = 1;
				break;
			case 'f':
				file_name = optarg;
				break;
			default:
				print_help();
				return 1;
		}
	}

	// Check that we got device name
	if (!device_name)
	{
		print_help();
		return 1;
	}

	// Check did we got file name
	if (file_name)
	{
		// Get file size
		if (stat(file_name, &st) != 0)
		{
			fprintf(stderr, "Error reading file size: %s\n", file_name);
			return 1;
		}

		// Allocate memory
		file_data = malloc(st.st_size);

		// Open file
		file_fd = open(file_name, O_RDONLY);
		if (file_fd < 0)
		{
			fprintf(stderr, "Error opening file: %s\n", file_name);
			return 1;
		}

		// Read data
		int data_read;
		int total_read = 0;

		while ((data_read = read(file_fd, file_data + total_read, st.st_size - total_read)) > 0)
		{
			if (extra_info)
			{
				printf("Read from file: %d bytes\n", data_read);
			}

			total_read += data_read;
		}

		close(file_fd);

		// Check that the whole file was read
		if (total_read != st.st_size)
		{
			fprintf(stderr, "Not all data was read from file. Size %d Read %d\n", (int)st.st_size, total_read);
			return 1;
		}
	}

	// Open device
	device_fd = open(device_name, O_RDWR);
	if (device_fd < 0)
	{
		fprintf(stderr, "Error opening device: %s\n", device_name);
		return 1;
	}

	// If sending file
	if (file_name)
	{
		// Send "Send" in exec command with file size. Bytes 6 - 13 (64bit)
		struct utp_cmd exec_send;
		memcpy(&exec_send, &exec, sizeof(exec_send));
		memcpy(exec_send.data + FILE_SIZE_OFFSET, &st.st_size, sizeof(uint32_t));

		ret = send_cmd(device_fd, &exec_send, command, strlen(command), NULL);
		if (ret)
		{
			fprintf(stderr, "Send_cmd failed (exec_send)\n");
		}

		// Send data in put command
		struct utp_cmd put_send;
		memcpy(&put_send, &put, sizeof(put_send));

		int data_written = 0;
		while (data_written < st.st_size)
		{
			int write_size = st.st_size - data_written > MAX_SENT_DATA_SIZE ? MAX_SENT_DATA_SIZE : st.st_size - data_written;

			ret = send_cmd(device_fd, &put_send, file_data + data_written, write_size, NULL);
			if (ret)
			{
				fprintf(stderr, "Send_cmd failed (put_send)\n");
				break;
			}

			data_written += write_size;
		}
	}
	else
	{
		// Call exec command
		ret = send_cmd(device_fd, &exec, command, strlen(command), NULL);
		if (ret)
		{
			fprintf(stderr, "Send_cmd failed (exec)\n");
		}

		// Wait until not busy
		int i;
		uint8_t reply = UTP_REPLY_BUSY;
		for (i = 0; i < BUSY_CHECK_COUNT; i++)
		{
			usleep(BUSY_SLEEP);

			ret = send_cmd(device_fd, &poll, "", 0, &reply);
			if (ret)
			{
				fprintf(stderr, "Send_cmd failed (poll)\n");
				break;
			}

			if (reply == 0)
			{
				break;
			}

			if (i == BUSY_CHECK_COUNT - 1)
			{
				fprintf(stderr, "Device is busy\n");
				ret = 1;
				break;
			}
		}
	}

	close(device_fd);

	return ret;
}
