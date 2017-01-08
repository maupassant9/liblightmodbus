/*
	liblightmodbus - a lightweight, multiplatform Modbus library
	Copyright (C) 2016	Jacek Wieczorek <mrjjot@gmail.com>

	This file is part of liblightmodbus.

	Liblightmodbus is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Liblightmodbus is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <lightmodbus/core.h>
#include <lightmodbus/parser.h>
#include <lightmodbus/master/mtypes.h>
#include <lightmodbus/master/mdiscreteinputs.h>

uint8_t modbusBuildRequest02( ModbusMaster *status, uint8_t address, uint16_t firstInput, uint16_t inputCount )
{
	//Build request02 frame, to send it so slave
	//Read multiple discrete inputs

	//Set frame length
	uint8_t frameLength = 8;

	//Check if given pointer is valid
	if ( status == NULL ) return MODBUS_ERROR_OTHER;

	//Set output frame length to 0 (in case of interrupts)
	status->request.length = 0;
	status->finished = 0;
	status->predictedResponseLength = 0;

	//Reallocate memory for final frame
	free( status->request.frame );
	status->request.frame = (uint8_t *) malloc( frameLength );
	if ( status->request.frame == NULL )
	{
		status->finished = 1;
		return MODBUS_ERROR_ALLOC;
	}
	union ModbusParser *builder = (union ModbusParser *) status->request.frame;

	builder->base.address = address;
	builder->base.function = 2;
	builder->request02.firstInput = modbusSwapEndian( firstInput );
	builder->request02.inputCount = modbusSwapEndian( inputCount );

	//Calculate crc
	builder->request02.crc = modbusCRC( builder->frame, frameLength - 2 );

	status->request.length = frameLength;
	status->predictedResponseLength = 4 + 2 + ( ( inputCount - 1 ) >> 3 );
	status->finished = 1;

	return 0;
}

uint8_t modbusParseResponse02( ModbusMaster *status, union ModbusParser *parser, union ModbusParser *requestParser )
{
	//Parse slave response to request 02 (read multiple discrete inputs)

	//Update frame length
	uint8_t frameLength;
	uint8_t dataok = 1;
	uint8_t i = 0;

	//Check if given pointers are valid
	if ( status == NULL || parser == NULL || requestParser == NULL ) return MODBUS_ERROR_OTHER;
	frameLength = 5 + parser->response02.byteCount;

	//Check frame crc
	dataok &= ( modbusCRC( parser->frame, frameLength - 2 ) & 0x00FF ) == parser->response02.values[parser->response02.byteCount];
	dataok &= ( ( modbusCRC( parser->frame, frameLength - 2 ) & 0xFF00 ) >> 8 ) == parser->response02.values[parser->response02.byteCount + 1];

	if ( !dataok )
	{
		status->finished = 1;
		return MODBUS_ERROR_CRC;
	}

	//Check between data sent to slave and received from slave
	dataok &= ( parser->base.address == requestParser->base.address );
	dataok &= ( parser->base.function == requestParser->base.function );

	//If data is bad abort parsing, and set error flag
	if ( !dataok )
	{
		status->finished = 1;
		return MODBUS_ERROR_FRAME;
	}

	free( status->data );
	status->data = (ModbusData *) malloc( sizeof( ModbusData ) * modbusSwapEndian( requestParser->request02.inputCount ) );
	if ( status->data == NULL )
	{
		status->finished = 1;
		return MODBUS_ERROR_ALLOC;
	}

	for ( i = 0; i < modbusSwapEndian( requestParser->request02.inputCount ); i++ )
	{
		status->data[i].address = parser->base.address;
		status->data[i].dataType =MODBUS_DISCRETE_INPUT;
		status->data[i].reg = modbusSwapEndian( requestParser->request02.firstInput ) + i;
		status->data[i].value = modbusMaskRead( parser->response02.values, parser->response02.byteCount, i );
		if ( status->data[i].value == 255 )
		{
			status->finished = 1;
			return MODBUS_ERROR_OTHER;
		}
	}

	//Set up data length - response successfully parsed
	status->dataLength = modbusSwapEndian( requestParser->request02.inputCount );
	status->finished = 1;

	return 0;
}
