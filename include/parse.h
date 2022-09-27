#pragma once
#include "config.h"
namespace Lemon {

/** Tries to parse a single log record.
@param[out]	type		log record type
@param[in]	ptr		pointer to a buffer
@param[in]	end_ptr		end of the buffer
@param[out]	space_id	tablespace identifier
@param[out]	page_no		page number
@param[out]	body		start of log record body
@return length of the record, or 0 if the record was not complete */
uint32_t
ParseSingleLogRecord(
    LOG_TYPE &type,
    const byte* ptr,
    const byte* end_ptr,
    space_id_t &space_id,
    page_id_t &page_id,
    byte** body);

}
