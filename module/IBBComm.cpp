/*****************************************************************************/
/*                                                                           */
/*    Copyright (C) - LEGATE Intelligent Equipment - All rights reserved     */
/*                                                                           */
/*****************************************************************************/
/*                                                                           */
/*  Except if expressly provided in a dedicated License Agreement, you are   */
/*  not authorized to:                                                       */
/*                                                                           */
/*  1. Use, copy, modify or transfer this software component, module or      */
/*  product, including any accompanying electronic or paper documentation    */
/*  (together, the "Software").                                              */
/*                                                                           */
/*  2. Remove any product identification, copyright, proprietary notices or  */
/*  labels from the Software.                                                */
/*                                                                           */
/*  3. Modify, reverse engineer, decompile, disassemble or otherwise attempt */
/*  to reconstruct or discover the source code, or any parts of it, from the */
/*  binaries of the Software.                                                */
/*                                                                           */
/*  4. Create derivative works based on the Software (e.g. incorporating the */
/*  Software in another software or commercial product or service without a  */
/*  proper license).                                                         */
/*                                                                           */
/*  By installing or using the "Software", you confirm your acceptance of the*/
/*  hereabove terms and conditions.                                          */
/*                                                                           */
/*****************************************************************************/


/*****************************************************************************/
/*  History:                                                                 */
/*****************************************************************************/
/*  Date       * Author          * Changes                                   */
/*****************************************************************************/
/*  2014-06-17 * ricky.gong      * Creation of the file                      */
/*             *                 *                                           */
/*****************************************************************************/


/*****************************************************************************/
/*                                                                           */
/*  Include Files                                                            */
/*                                                                           */
/*****************************************************************************/
#include <stdio.h>
#include <string.h>

#include "sync.h"
#include "msgQueue.h"
#include "thread.h"
#include "serial.h"
#include "IBBComm.h"


/*****************************************************************************/
/*                                                                           */
/*  Definitions                                                              */
/*                                                                           */
/*****************************************************************************/
#define ACK_TIMEOUT      20
#define RSP_TIMEOUT      20
#define RSP_BOOST_TIMEOUT 300

#define MAX_FRAM_TRANSMIT_COUNT    2

#define SIZE_OF_RECEIVE_BUF      (1024)
#define SIZE_OF_TRANSMIT_BUF     (512)
#define SIZE_OF_COMMAND_BUF      (256)


/*****************************************************************************/
/*                                                                           */
/*  Variable Declarations                                                    */
/*                                                                           */
/*****************************************************************************/


/*****************************************************************************/
/*                                                                           */
/*  Function Declarations                                                    */
/*                                                                           */
/*****************************************************************************/
#if defined (__cplusplus)
#include <time.h>

extern "C"
{
#endif

#if defined(__linux)
unsigned long GetTickCount()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
#endif

#if defined (__cplusplus)
} // end of extern "C"
#endif


/*****************************************************************************/
/*                                                                           */
/*  Function Implementations                                                 */
/*                                                                           */
/*****************************************************************************/
/*****************************************************************************/
/* Function Description:                                                     */
/*****************************************************************************/
/*   Compute the LRC value                                                   */
/*                                                                           */
/*****************************************************************************/
/* Parameters:                                                               */
/*****************************************************************************/
/*   pData[in]:             data buffer to compute on                        */
/*   nbBytes[in]:           data in bytes                                    */
/*                                                                           */
/*****************************************************************************/
/* Return Values:                                                            */
/*****************************************************************************/
/*   the result value                                                        */
/*                                                                           */
/*****************************************************************************/
static unsigned char calculateLRC(const unsigned char* pData, size_t nbBytes)
{
    unsigned char XOR = 0;

    for(size_t count = 0; count < nbBytes; count++)
      XOR ^= pData[count];

    return XOR;
}

/*****************************************************************************/
/* Function Description:                                                     */
/*****************************************************************************/
/*   Constructor                                                             */
/*                                                                           */
/*****************************************************************************/
/* Parameters:                                                               */
/*****************************************************************************/
/*   none                                                                    */
/*                                                                           */
/*****************************************************************************/
/* Return Values:                                                            */
/*****************************************************************************/
/*   none                                                                    */
/*                                                                           */
/*****************************************************************************/
CIBBCommProto::CIBBCommProto()
    : CSerial(),
      ackTimeout(ACK_TIMEOUT),
      rspTimeout(RSP_TIMEOUT),
      eFrameSM(SM_NO_FRAME_TO_RECIEVE),
      RecvMsgQ(64),
      stopQueue(false)
{
    //create the Thread
    CThread::initInstance("IORecv",THREAD_DEFAULT_STACK_SIZE, THREAD_MAX_PRIORITY);
}

/*****************************************************************************/
/* Function Description:                                                     */
/*****************************************************************************/
/*   Desstructor                                                             */
/*                                                                           */
/*****************************************************************************/
/* Parameters:                                                               */
/*****************************************************************************/
/*   none                                                                    */
/*                                                                           */
/*****************************************************************************/
/* Return Values:                                                            */
/*****************************************************************************/
/*   none                                                                    */
/*                                                                           */
/*****************************************************************************/
CIBBCommProto::~CIBBCommProto()
{
    if (CThread::getExitCode() == RUNNING) {
        CThread::stop();
        CThread::join();
    }
}

/*****************************************************************************/
/* Function Description:                                                     */
/*****************************************************************************/
/*   Impelementation code of the thread                                      */
/*                                                                           */
/*****************************************************************************/
/* Parameters:                                                               */
/*****************************************************************************/
/*   none                                                                    */
/*                                                                           */
/*****************************************************************************/
/* Return Values:                                                            */
/*****************************************************************************/
/*   The exit code                                                           */
/*                                                                           */
/*****************************************************************************/
int CIBBCommProto::run(void)
{
    unsigned char tReceiveBuf[SIZE_OF_RECEIVE_BUF];
    unsigned char tCurCmd[SIZE_OF_COMMAND_BUF];
    unsigned long readBytes = 0;
    unsigned char *pCurPtr = tReceiveBuf;

    while(!m_stopRequest)
    {
        // check if there's any data available and the data buffer is not full
        if(pCurPtr < (tReceiveBuf + SIZE_OF_RECEIVE_BUF))
        {
            if((readBytes = read((char*)pCurPtr, 1)) > 0)
            {
                // ignore data receiving if it no need to receive data in these cases
                if((eFrameSM == SM_NO_FRAME_TO_RECIEVE) 
                    || (eFrameSM == SM_RSP_FRAME_RECIEVED) 
                    || (eFrameSM == SM_ACK_FRAME_RECIEVED) 
                    || (eFrameSM == SM_NAK_FRAME_RECIEVED)) 
                {
                    continue;
                }

                size_t pos = 0;

                pCurPtr += readBytes;

                // check for the 1st DLE character to ignore the non-DLE 
                // characters before it
                // also check it's not arriving the end of valid data
                while(&tReceiveBuf[pos] < pCurPtr)
                {
                    if(tReceiveBuf[pos] == DLE_CHARACTER)
                    {
                        // check if the DLE character is the 1st character in received 
                        // data buffer, if not, remove the previous data
                        if(pos != 0)
                        {
                            size_t mvsize = pCurPtr - &tReceiveBuf[pos];
                            memmove(&tReceiveBuf[0], &tReceiveBuf[pos], mvsize);
                            pCurPtr -= pos;
                            pos = 0;
                        }

                        // here we've make sure the 1st character is DLE, so then
                        // check the 2nd character (make sure the 2nd character has 
                        // already received)
                        if((pCurPtr - tReceiveBuf) > 1)
                        {
                            switch(tReceiveBuf[1])
                            {
                                case ACK_CHARACTER:
                                    // received the ACK package frame
                                    if(eFrameSM == SM_CMD_FRAME_TRANSMITTED) 
                                    {
                                        eFrameSM = SM_ACK_FRAME_RECIEVED;
                                    }
                                    else
                                    {
                                        // corrupted data found, just ignore the DLE character
                                        tReceiveBuf[0] = 0x00;
                                    }
                                break;

                                case NAK_CHARACTER:
                                    // received the ACK package frame
                                    if(eFrameSM == SM_CMD_FRAME_TRANSMITTED) 
                                    {
                                        eFrameSM = SM_NAK_FRAME_RECIEVED;
                                    }
                                    else
                                    {
                                        // corrupted data found, just ignore the DLE character
                                        tReceiveBuf[0] = 0x00;
                                    }
                                break;

                                case STX_CHARACTER:
                                    // received the command response package frame start 
                                    if((eFrameSM == SM_ENQ_FRAME_TRANSMITTED) 
                                       || (eFrameSM == SM_RSP_FRAME_STX_RECIEVED))
                                    {
                                        eFrameSM = SM_RSP_FRAME_STX_RECIEVED;
                                    }
                                    else
                                    {
                                        // corrupted data found, just ignore the DLE character
                                        tReceiveBuf[0] = 0x00;
                                    }
                                break;

                                default:
                                    // corrupted data found, just ignore the DLE character
                                    tReceiveBuf[0] = 0x00;
                                break;
                            }
                        }
                        break;
                    }
                    pos++;
                }

                if(eFrameSM == SM_RSP_FRAME_STX_RECIEVED)
                {
                    bool bFrameOK = false;

                    while(&tReceiveBuf[pos] < pCurPtr) 
                    {
                        // here the 1st and 2nd characters are DLE STX, also the next DLE EXT 
                        // or DLE STX flag will exist in 4 characters, so start checking from 
                        // the 4th characters
                        if(pos > 3)
                        {
                            // check for the end flag (DLE ETX with no additioal DLE
                            // before itor with multiple double DLE prefixed) with checksum
                            if((tReceiveBuf[pos-2] == DLE_CHARACTER) && (tReceiveBuf[pos-1] == ETX_CHARACTER))
                            {
                                if(tReceiveBuf[pos-3] != DLE_CHARACTER)
                                {
                                    pos++;
                                    // no DLE_CHARACTER before it
                                    bFrameOK = true;
                                    break;
                                }
                                else
                                {
                                    int tmppos = pos - 3;
                                    size_t dlePrefixNum = 0;
                                    while((tReceiveBuf[tmppos] == DLE_CHARACTER) && (tmppos >= 0))
                                    {
                                        dlePrefixNum++;
                                        tmppos--;
                                    }
                                    if(dlePrefixNum%2 == 0)
                                    {
                                        pos++;
                                        // with multiple double DLE prefixed
                                        bFrameOK = true;
                                        break;
                                    }
                                }
                            }
                            // check for the start character (DLE STX with no additioal DLE
                            // before it or with multiple double DLE prefixed) because of 
                            // error data or data lost maybe occurred
                            else if((tReceiveBuf[pos-1] == DLE_CHARACTER) && (tReceiveBuf[pos] == STX_CHARACTER))
                            {
                                if(tReceiveBuf[pos-2] != DLE_CHARACTER)
                                {
                                    // discard the former data without valid frame of end 
                                    // character
                                    size_t mvsize = pCurPtr - &tReceiveBuf[pos-1];
                                    memmove(&tReceiveBuf[0], &tReceiveBuf[pos-1], mvsize);
                                    pCurPtr -= (pos-1);
                                    pos = 0;
                                    // continue to restart checking loop of the DLE ETX flags
                                    continue;
                                }
                                else
                                {
                                    int tmppos = pos - 2;
                                    size_t dlePrefixNum = 0;
                                    while((tReceiveBuf[tmppos] == DLE_CHARACTER) && (tmppos >= 0))
                                    {
                                        dlePrefixNum++;
                                        tmppos--;
                                    }
                                    if(dlePrefixNum%2 == 0)
                                    {
                                        // discard the former data without valid frame of end 
                                        // character
                                        size_t mvsize = pCurPtr - &tReceiveBuf[pos-1];
                                        memmove(&tReceiveBuf[0], &tReceiveBuf[pos-1], mvsize);
                                        pCurPtr -= (pos-1);
                                        pos = 0;
                                        // continue to restart checking loop of the DLE ETX flags
                                        continue;
                                    }
                                }
                            }
                        }
                        pos++;
                    }

                    if(bFrameOK) 
                    {
                        size_t mvsize;

                        bool bCmdValid = false;
                        // data field of the command, "DLE STX + Command Data Field + DLE ETX + BCC Checksum"
                        size_t dataFieldSize = pos - 5;
                        unsigned char checksum = tReceiveBuf[pos-1];

                        // check the checksum
                        if(calculateLRC(&tReceiveBuf[2], dataFieldSize) == checksum)
                        {
                            size_t tmpsz = 0;

                            memcpy(tCurCmd, &tReceiveBuf[2], dataFieldSize);
                            // check DLE (skip the last character to avoid DLE in last byte 
                            // because DLE character in data field should always prefixed 
                            // with another DLE character)
                            while(tmpsz < (dataFieldSize-1)) 
                            {
                                // skip DLE length to checksum
                                if(tCurCmd[tmpsz] == DLE_CHARACTER) 
                                {
                                    // move 1 byte to remove the DLE
                                    mvsize = dataFieldSize - tmpsz;
                                    if(mvsize > 0) 
                                    {
                                        memmove(&tCurCmd[tmpsz], &tCurCmd[tmpsz+1], mvsize-1);
                                    }
                                    dataFieldSize--; // move back the rear pointer 1 byte 
                                }
                                tmpsz++;
                            }

                            // analyse a data field size only if the field is at least 2 bytes (
                            // LEN + CMD + PARAM (optional))
                            if((dataFieldSize >= 2) && (dataFieldSize == (size_t)(tCurCmd[0] + 1)))
                            {
                                bCmdValid = bCmdValcheck(tCurCmd[0], tCurCmd[1], &tCurCmd[2]);
                                if(bCmdValid)
                                {
                                    StCmdStruct recData;
                                    memcpy(&recData, tCurCmd, tCurCmd[0]+1);
                                    RecvMsgEnqueue(recData);
                                    eFrameSM = SM_RSP_FRAME_RECIEVED;
                                }
                            }
                        }

                        // discard the former data with valid frame of end character
                        mvsize = pCurPtr - &tReceiveBuf[pos];
                        memmove(&tReceiveBuf[0], &tReceiveBuf[pos], mvsize);
                        pCurPtr -= pos;
                    }
                }
                else if(eFrameSM == SM_ACK_FRAME_RECIEVED)
                {
                    StCmdStruct recData;
                    memset(&recData, 0x00, sizeof(recData));
                    recData.len = sizeof(recData.cmd);
                    recData.cmd = CMD_ACK;
                    // enqueue an ACK command
                    RecvMsgEnqueue(recData);

                    // discard the DLE ENQ bytes
                    size_t mvsize = pCurPtr - &tReceiveBuf[2];
                    memmove(&tReceiveBuf[0], &tReceiveBuf[2], mvsize);
                    pCurPtr -= 2;
                }
                else if(eFrameSM == SM_NAK_FRAME_RECIEVED)
                {
                    StCmdStruct recData;
                    memset(&recData, 0x00, sizeof(recData));
                    recData.len = sizeof(recData.cmd);
                    recData.cmd = CMD_NAK;
                    // enqueue an NAK command
                    RecvMsgEnqueue(recData);

                    // discard the DLE ENQ bytes
                    size_t mvsize = pCurPtr - &tReceiveBuf[2];
                    memmove(&tReceiveBuf[0], &tReceiveBuf[2], mvsize);
                    pCurPtr -= 2;
                }
            }
        }
        else
        {
            // the buffer is full, force set the current pointer back to the 
            // start of the recieve buffer
            pCurPtr = tReceiveBuf;
        }
    }

    return SUCCESS;
}

/*****************************************************************************/
/* Function Description:                                                     */
/*****************************************************************************/
/*   Start IBB communication port                                            */
/*                                                                           */
/*****************************************************************************/
/* Parameters:                                                               */
/*****************************************************************************/
/*   pszComName[in]:        com port name string                             */
/*   ulSpeed[in]:           com port baud rate                               */
/*                                                                           */
/*****************************************************************************/
/* Return Values:                                                            */
/*****************************************************************************/
/*   ERR_CODE_NONE - operation with no error                                 */
/*                                                                           */
/*****************************************************************************/
int CIBBCommProto::startIOComm(const char* pszComName, unsigned long ulSpeed)
{
    int nRet = ERR_CODE_COMM_INST_ERR;

    commTxnMutex.take();

    if(CSerial::initInstance(pszComName, ulSpeed, HARD_OPT_CS8, RS232))
    {
        nRet = ERR_CODE_NONE;
    }

    commTxnMutex.release();

    if(nRet == ERR_CODE_NONE)
    {
        // start date time display thread
        start();

        // wait to syncronous threads
        waitStarted();
    }
    return nRet;
}

/*****************************************************************************/
/* Function Description:                                                     */
/*****************************************************************************/
/*   Stop IO communication port                                              */
/*                                                                           */
/*****************************************************************************/
/* Parameters:                                                               */
/*****************************************************************************/
/*   none                                                                    */
/*                                                                           */
/*****************************************************************************/
/* Return Values:                                                            */
/*****************************************************************************/
/*   ERR_CODE_NONE - operation with no error                                 */
/*                                                                           */
/*****************************************************************************/
int CIBBCommProto::StopIOComm()
{
    int nRet = ERR_CODE_NONE;
    //stop the thread first
    m_stopRequest = true;

    // prepare a pseudo command and enqueue into message queue to exit message 
    // dequeue waiting
    StCmdStruct recData;
    memset(&recData, 0x00, sizeof(recData));
    recData.len = sizeof(recData.cmd);
    recData.cmd = CMD_PSEUDO_CMD;
    RecvMsgEnqueue(recData);

    if (CThread::getExitCode() == RUNNING) {
        CThread::stop();
        //about serial read to prepare to destruct serial instance

        commTxnMutex.take();

        abortRead();

        commTxnMutex.release();

        CThread::join();
    }
    return nRet;
}

/*****************************************************************************/
/* Function Description:                                                     */
/*****************************************************************************/
/*   Command send to the IO board                                            */
/*                                                                           */
/*****************************************************************************/
/* Parameters:                                                               */
/*****************************************************************************/
/*   stData[in/out]:        data structure to store data to be sent and      */
/*                          data received                                    */
/*                                                                           */
/*****************************************************************************/
/* Return Values:                                                            */
/*****************************************************************************/
/*   ERR_CODE_NONE - operation with no error                                 */
/*   ERR_CODE_OPERATE_TIMEOUT - operation timeout                            */
/*                                                                           */
/*****************************************************************************/
int CIBBCommProto::cmdSend(StCmdStruct &stData)
{
    int nRet = ERR_CODE_NONE;
    unsigned char tTransmitBuf[SIZE_OF_TRANSMIT_BUF];
    unsigned char curCmd = stData.cmd;

    commTxnMutex.take();

    size_t index;
    size_t dataSize = 0;

    // response data frame "DLE STX + Command Data Field + DLE ETX + BCC Checksum"
    tTransmitBuf[dataSize++] = (unsigned char)DLE_CHARACTER;
    tTransmitBuf[dataSize++] = (unsigned char)STX_CHARACTER;
    tTransmitBuf[dataSize++] = stData.len;
    tTransmitBuf[dataSize++] = stData.cmd;
    memcpy(&tTransmitBuf[dataSize], &(stData.Data), stData.len-1);
    dataSize += (stData.len-1);

    // insert DLE after DLE data if there's 1 byte DLE data in the response packet data
    for(index=2; index < (size_t)(stData.len+3); index++) 
    {
        if(tTransmitBuf[index] == (unsigned char)DLE_CHARACTER) 
        {
            memmove(&tTransmitBuf[index+2], &tTransmitBuf[index+1], 
                    dataSize-index);
            tTransmitBuf[index+1] = (unsigned char)DLE_CHARACTER;
            index++;
            dataSize++;
        }
    }
    tTransmitBuf[dataSize++] = (unsigned char)DLE_CHARACTER;
    tTransmitBuf[dataSize++] = (unsigned char)ETX_CHARACTER;
    tTransmitBuf[dataSize] = calculateLRC(&tTransmitBuf[2], dataSize-4);
    dataSize++;

    int nTransmitCnt = 0;
    // send until ACK frame received or arrives the maximun retransmit time
    do {
    #if 1
        if(nTransmitCnt > 0)
            printf("[%ld] Cmd(0x%02X) ACK re-send: %d\n", GetTickCount(), curCmd, nTransmitCnt);
    #endif // 
        CSerial::write((char*)tTransmitBuf, dataSize);
        eFrameSM = SM_CMD_FRAME_TRANSMITTED;
    } while(!(RecvMsgDequeue(stData, ackTimeout) && (stData.cmd == CMD_ACK)) 
            && (++nTransmitCnt < MAX_FRAM_TRANSMIT_COUNT) && (!stopQueue));

    // ACK frame received, then ENQ frame to send and wait for operation response
    if(eFrameSM == SM_ACK_FRAME_RECIEVED)
    {
        tTransmitBuf[0] = (unsigned char)DLE_CHARACTER;
        tTransmitBuf[1] = (unsigned char)ENQ_CHARACTER;

        nTransmitCnt = 0;

        // used boost timeout because get SN information command uses RSA algorithm
        // and needs more time to compute the result
        if(curCmd == CMD_GET_SN_INFO) {
            rspTimeout = RSP_BOOST_TIMEOUT;
        } else {
            rspTimeout = RSP_TIMEOUT;
        }
        // send until ACK frame received or arrives the maximun retransmit time
        do {
        #if 1
            if(nTransmitCnt > 0)
                printf("[%ld] Cmd(0x%02X) RSP re-send: %d\n", GetTickCount(), curCmd, nTransmitCnt);
        #endif // 
            CSerial::write((char*)tTransmitBuf, 2);
            eFrameSM = SM_ENQ_FRAME_TRANSMITTED;
        } while(!(RecvMsgDequeue(stData, rspTimeout) && (stData.cmd == curCmd)) 
                && (++nTransmitCnt < MAX_FRAM_TRANSMIT_COUNT) && (!stopQueue));
    }

    if(nTransmitCnt >= MAX_FRAM_TRANSMIT_COUNT)
    {
        nRet = ERR_CODE_OPERATE_TIMEOUT;
    }

    // flush serial port read buffer to avoid corrupted data mixed into next command
    if(nRet != ERR_CODE_NONE) 
    {
        flushReadBuffer();
    }

    commTxnMutex.release();

    return nRet;
}

/*****************************************************************************/
/* Function Description:                                                     */
/*****************************************************************************/
/*   Enqueue received messages                                               */
/*                                                                           */
/*****************************************************************************/
/* Parameters:                                                               */
/*****************************************************************************/
/*   recData[out]:          one received message record                      */
/*                                                                           */
/*****************************************************************************/
/* Return Values:                                                            */
/*****************************************************************************/
/*   none                                                                    */
/*                                                                           */
/*****************************************************************************/
void CIBBCommProto::RecvMsgEnqueue(StCmdStruct &recData)
{
    if(!stopQueue)
    {
        StCmdStruct *pStData = new StCmdStruct;
        // enqueue message
        if(pStData != NULL)
        {
            memcpy(pStData, &recData, sizeof(StCmdStruct));
            RecvMsgQ.enqueue(pStData);
        }
    }
}

/*****************************************************************************/
/* Function Description:                                                     */
/*****************************************************************************/
/*   Dequeue received messages                                               */
/*                                                                           */
/*****************************************************************************/
/* Parameters:                                                               */
/*****************************************************************************/
/*   recData[out]:          one received message record                      */
/*   timeout[in]:           timeout in ms unit                               */
/*                                                                           */
/*****************************************************************************/
/* Return Values:                                                            */
/*****************************************************************************/
/*   true - a message dequeued ok                                            */
/*   false - no message available                                            */
/*                                                                           */
/*****************************************************************************/
bool CIBBCommProto::RecvMsgDequeue(StCmdStruct &recData, int timeout)
{
    bool bRet = false;
    if(!stopQueue)
    {
        // dequeue log message
        StCmdStruct *pStData = NULL;

        if(RecvMsgQ.dequeue(pStData, timeout) && (pStData != NULL))
        {
            memcpy(&recData, pStData, sizeof(StCmdStruct));
            delete pStData;
            bRet = true;
        }
        if(bRet && (recData.cmd == CMD_PSEUDO_CMD))
        {
            stopQueue = true;
        }
    }
    return bRet;
}

/*****************************************************************************/
/* Function Description:                                                     */
/*****************************************************************************/
/*   check if the command data is valid                                      */
/*                                                                           */
/*****************************************************************************/
/* Parameters:                                                               */
/*****************************************************************************/
/*   len[in]:              length of the command                             */
/*   cmd[in]:              command byte                                      */
/*   param[in]:            parameters of the command                         */
/*                                                                           */
/*****************************************************************************/
/* Return Values:                                                            */
/*****************************************************************************/
/*   true - the command data is valid                                        */
/*   false - the command data is invalid                                     */
/*                                                                           */
/*****************************************************************************/
bool CIBBCommProto::bCmdValcheck(unsigned char len, unsigned char cmd, unsigned char* param)
{
    bool bRet = false;
    switch(cmd)
    {
        case CMD_GET_IO_INPUT:
            if(len == 8)
            {
                bRet = true;
            }
        break;

        case CMD_SET_IO_OUTPUT:
            if(len == 1)
            {
                bRet = true;
            }
        break;

        case CMD_IO_IN_OUT_GETSET:
            if(len == 8)
            {
                bRet = true;
            }
            break;

        case CMD_SEN_TRIG_LVL:
            if(len == 1)
            {
                bRet = true;
            }
            break;

        case CMD_GET_VERSION_INFO:
            if(len == 12)
            {
                bRet = true;
            }
        break;

        case CMD_GET_SN_INFO:
            if(len == 65)
            {
                bRet = true;
            }
        break;

        case CMD_RESET_IB:
            if(len == 1)
            {
                bRet = true;
            }
        break;

        default:
        break;
    }

    return bRet;
}

