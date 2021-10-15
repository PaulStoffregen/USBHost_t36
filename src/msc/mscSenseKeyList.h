/* Copyright 2013 Baruch Even
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef MSC_SENSE_KEY_LIST_H
#define MSC_SENSE_KEY_LIST_H

#define SENSE_KEY_LIST \
        SENSE_KEY_MAP(NO_SENSE, 0x0) \
        SENSE_KEY_MAP(RECOVERED_ERROR, 0x1) \
        SENSE_KEY_MAP(NOT_READY, 0x2) \
        SENSE_KEY_MAP(MEDIUM_ERROR, 0x3) \
        SENSE_KEY_MAP(HARDWARE_ERROR, 0x4) \
        SENSE_KEY_MAP(ILLEGAL_REQUEST, 0x5) \
        SENSE_KEY_MAP(UNIT_ATTENTION, 0x6) \
        SENSE_KEY_MAP(DATA_PROTECT, 0x7) \
        SENSE_KEY_MAP(BLANK_CHECK, 0x8) \
        SENSE_KEY_MAP(VENDOR_SPECIFIC, 0x9) \
        SENSE_KEY_MAP(COPY_ABORTED, 0xA) \
        SENSE_KEY_MAP(ABORTED_COMMAND, 0xB) \
        SENSE_KEY_MAP(RESERVED_C, 0xC) \
        SENSE_KEY_MAP(VOLUME_OVERFLOW, 0xD) \
        SENSE_KEY_MAP(MISCOMPARE, 0xE) \
        SENSE_KEY_MAP(COMPLETED, 0xF)

#endif
