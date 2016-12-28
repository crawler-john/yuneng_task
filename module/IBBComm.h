#ifndef _IBBCOMM_H_
#define _IBBCOMM_H_
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
/*  2014-06-18 * ricky.gong      * Creation of the file                      */
/*             *                 *                                           */
/*****************************************************************************/


/*****************************************************************************/
/*                                                                           */
/*  Include Files                                                            */
/*                                                                           */
/*****************************************************************************/
#include "IBBApi.h"


/*****************************************************************************/
/*                                                                           */
/*  Definitions                                                              */
/*                                                                           */
/*****************************************************************************/
// special characters
#define DLE_CHARACTER    0x10
#define STX_CHARACTER    0x02
#define ETX_CHARACTER    0x03
#define ENQ_CHARACTER    0x05
#define ACK_CHARACTER    0x06
#define NAK_CHARACTER    0x15

// commands
// communication command
#define CMD_GET_IO_INPUT     0x20
#define CMD_SET_IO_OUTPUT    0x21
#define CMD_IO_IN_OUT_GETSET 0x22
#define CMD_SEN_TRIG_LVL     0x23
#define CMD_GET_VERSION_INFO 0x70
#define CMD_GET_SN_INFO      0x71
#define CMD_RESET_IB         0x7F
#define CMD_ACK              0xF1
#define CMD_NAK              0xF2
#define CMD_PSEUDO_CMD       0xFF

// state machine of the com port data receive
typedef enum 
{
    SM_NO_FRAME_TO_RECIEVE    = 0,
    SM_CMD_FRAME_TRANSMITTED  = 1,
    SM_ACK_FRAME_RECIEVED     = 2,
    SM_NAK_FRAME_RECIEVED     = 3,
    SM_ENQ_FRAME_TRANSMITTED  = 4,
    SM_RSP_FRAME_STX_RECIEVED = 5,
    SM_RSP_FRAME_RECIEVED     = 6,
} eFrameRecvSM;

#pragma pack(push,1)
typedef struct
{
    unsigned char len;
    unsigned char cmd;
    union 
    {
        union 
        {
            unsigned char  tucInput[7];
            unsigned char  firmwareVer[11];
            unsigned char  tucEncSN[64];
        } RecvData;
        union 
        {
            unsigned char  tucOutput[4];
            unsigned char  tucRandom[8];
            unsigned char  ucSenTrigLvl;
        } SendData;
    } Data;
} StCmdStruct;
#pragma pack(pop)

/*****************************************************************************/
/* Class Description:                                                        */
/*****************************************************************************/
/*   Class packed with IO communication functions                            */
/*                                                                           */
/*****************************************************************************/
class CIBBCommProto : public CSerial, public CThread
{
    public:

        /*********************************************************************/
        /*                     Constructor & Destructor                      */
        /*********************************************************************/
        CIBBCommProto();
        ~CIBBCommProto();


        /*********************************************************************/
        /*                            Variables                              */
        /*********************************************************************/


        /*********************************************************************/
        /*                            Functions                              */
        /*********************************************************************/
        int startIOComm(const char* pszComName, unsigned long ulSpeed);
        int StopIOComm();
        int cmdSend(StCmdStruct &stData);
        void RecvMsgEnqueue(StCmdStruct &recData);
        bool RecvMsgDequeue(StCmdStruct &recData, int timeout);
        bool bCmdValcheck(unsigned char len, unsigned char cmd, 
                          unsigned char* param);


    private:

        /*********************************************************************/
        /*                            Variables                              */
        /*********************************************************************/
        unsigned int ackTimeout;
        unsigned int rspTimeout;

        eFrameRecvSM eFrameSM;

        CMutex commTxnMutex;
        CMsgQueueT<StCmdStruct*> RecvMsgQ;
        volatile bool stopQueue;


        /*********************************************************************/
        /*                            Functions                              */
        /*********************************************************************/
        int run();


    protected:

        /*********************************************************************/
        /*                            Variables                              */
        /*********************************************************************/


        /*********************************************************************/
        /*                            Functions                              */
        /*********************************************************************/


};


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


#endif  // _IBBCOMM_H_

