#ifndef __DLMS_ERROR_H__
#define __DLMS_ERROR_H__

// Data access result code

#define DLMSRES_DATA_SUCCESS				0
#define DLMSRES_DATA_HARDWARE_FAULT			1
#define DLMSRES_DATA_TEMPORARY_FAIL			2
#define DLMSRES_DATA_READ_WRITE_DENIED		3
#define DLMSRES_DATA_OBJECT_UNDEFINED		4

// Action result code


#define DLMSERR_NOERROR						0		// No error

#define DLMSERR_INVALID_PARAM				-50
#define DLMSERR_INVALID_SESSION				-51
#define DLMSERR_INVALID_HANDLE				-52
#define DLMSERR_INVALID_ADDRESS				-53
#define DLMSERR_INVALID_PORT				-54
#define DLMSERR_INVALID_SOURCE_ADDR			-55
#define DLMSERR_INVALID_DEST_ADDR			-56
#define DLMSERR_INVALID_TIMEOUT				-57
#define DLMSERR_INVALID_ACCESS				-58
#define DLMSERR_INVALID_DATA				-59
#define DLMSERR_INVALID_ACTION				-60
#define DLMSERR_INVALID_OBISCODE			-61
#define DLMSERR_INVALID_STRING				-62
#define DLMSERR_INVALID_LENGTH				-63
#define DLMSERR_INVALID_BUFFER				-64

#define DLMSERR_CANNOT_CONNECT				-100
#define DLMSERR_REPLY_TIMEOUT				-101
#define DLMSERR_NOT_INITIALIZED				-102

#define DLMSERR_SYSTEM_ERROR				-200
#define DLMSERR_MEMORY_ERROR				-201

#endif	// __DLMS_ERROR_H__