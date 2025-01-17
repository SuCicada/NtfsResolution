// ConsoleApplication.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "ntfs.h"
#include "fat32.h"

using namespace std;

//#pragma comment(lib, "Shlwapi.lib")
//#pragma comment(lib, "Kernel32.lib")
//#pragma comment(lib, "version.lib")
#pragma pack(1)
#pragma warning(disable : 4996)


typedef struct _PHY_INFO {
    DWORD number;
    vector<TCHAR> vols;
} PHY_INFO, *pPHY_INFO;

typedef struct _VCN_LCN_SIZE {
    UINT64 VCN;
    UINT64 LCN;
    UINT64 SIZE;

    _VCN_LCN_SIZE() {}

    _VCN_LCN_SIZE(UINT64 vcn, UINT64 lcn, UINT64 size) {
        this->VCN = vcn;
        this->LCN = lcn;
        this->SIZE = size;
    }

    ~_VCN_LCN_SIZE() {}
} VCN_LCN_SIZE, *pVCN_LCN_SIZE;

FILE *fp;

//error msg
void GetErrorMessage(DWORD dwErrCode, DWORD dwLanguageId) {//dwLanguageId=0
    DWORD dwRet = 0;
    LPTSTR szResult = NULL;
    setlocale(LC_ALL, "chs");
    dwRet = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL,
                          dwErrCode, dwLanguageId, (LPTSTR) &szResult, 0, NULL);
    if (dwRet == 0) {
        szResult = NULL;
        _tprintf(_T("No such errorCode\n"));
    }
    else { _tprintf(_T("%s"), szResult); }
    szResult = NULL;
    return;
}


//char to WCHAR
WCHAR *charToWCHAR(char *s) {
    int w_nlen = MultiByteToWideChar(CP_ACP, 0, s, -1, NULL, 0);
    WCHAR *ret;
    ret = (WCHAR *) malloc(sizeof(WCHAR) * w_nlen);
    memset(ret, 0, sizeof(ret));
    MultiByteToWideChar(CP_ACP, 0, s, -1, ret, w_nlen);
    return ret;
}

//读取物理磁盘
//物理磁盘设备号 起始偏移(Byte) 读取长度(最小一个扇区) 输出缓冲
DWORD ReadDisk(DWORD physicalDriverNumber, UINT64 startOffset, DWORD size, PVOID ret) {
    OVERLAPPED over = {0};
    over.Offset = startOffset & (0xFFFFFFFF);
    over.OffsetHigh = (startOffset >> 32) & (0xFFFFFFFF);
    CHAR PHYSICALDRIVE[MAX_PATH];
    memset(PHYSICALDRIVE, 0, sizeof(PHYSICALDRIVE));
    strcpy(PHYSICALDRIVE, "\\\\.\\PHYSICALDRIVE");
    PHYSICALDRIVE[strlen(PHYSICALDRIVE)] = '0' + physicalDriverNumber;
    LPCWSTR PD = charToWCHAR(PHYSICALDRIVE);
    HANDLE handle = CreateFile(PD, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (handle == INVALID_HANDLE_VALUE) return 0;
    DWORD readsize;
    if (ReadFile(handle, ret, size, &readsize, &over) == 0) {
        CloseHandle(handle);
        return 0;
    }
    CloseHandle(handle);
    return readsize;
}

//根据逻辑分区获取其物理磁盘设备号
DWORD GetPhysicalDriveFromPartitionLetter(TCHAR letter) {
    HANDLE hDevice;               // handle to the drive to be examined
    BOOL result;                 // results flag
    DWORD readed;                   // discard results
    STORAGE_DEVICE_NUMBER number;   //use this to get disk numbers

    CHAR path[MAX_PATH];
    sprintf(path, "\\\\.\\%c:", letter);
    //printf("%s\n", path);
    hDevice = CreateFile(charToWCHAR(path), // drive to open
                         GENERIC_READ | GENERIC_WRITE,    // access to the drive
                         FILE_SHARE_READ | FILE_SHARE_WRITE,    //share mode
                         NULL,             // default security attributes
                         OPEN_EXISTING,    // disposition
                         0,                // file attributes
                         NULL);            // do not copy file attribute
    if (hDevice == INVALID_HANDLE_VALUE) // cannot open the drive
    {
        GetErrorMessage(GetLastError(), 0);
        //fprintf(stderr, "CreateFile() Error: %ld\n", GetLastError());
        return DWORD(-1);
    }

    result = DeviceIoControl(
            hDevice,                // handle to device
            IOCTL_STORAGE_GET_DEVICE_NUMBER, // dwIoControlCode
            NULL,                            // lpInBuffer
            0,                               // nInBufferSize
            &number,           // output buffer
            sizeof(number),         // size of output buffer
            &readed,       // number of bytes returned
            NULL      // OVERLAPPED structure
    );
    if (!result) // fail
    {
        fprintf(stderr, "IOCTL_STORAGE_GET_DEVICE_NUMBER Error: %ld\n", GetLastError());
        (void) CloseHandle(hDevice);
        return (DWORD) -1;
    }
    //printf("DeviceType(设备类型) is %d, DeviceNumber(物理设备号) is %d, PartitionNumber(分区号) is %d\n", number.DeviceType, number.DeviceNumber, number.PartitionNumber);

    (void) CloseHandle(hDevice);
    return number.DeviceNumber;
}

//获取逻辑分区的信息如卷标、空间等
void getVolumeInfo(LPCWSTR volumeName) {
    DWORD dwTotalClusters;//总的簇
    DWORD dwFreeClusters;//可用的簇
    DWORD dwSectPerClust;//每个簇有多少个扇区
    DWORD dwBytesPerSect;//每个扇区有多少个字节
    BOOL bResult = GetDiskFreeSpace((volumeName), &dwSectPerClust, &dwBytesPerSect, &dwFreeClusters, &dwTotalClusters);
    printf(("总簇数:%d\n可用簇数:%d\n簇内扇区数:%d\n扇区内字节数:%d\n"), dwTotalClusters, dwFreeClusters, dwSectPerClust, dwBytesPerSect);
    //GetErrorMessage(GetLastError(),0);
}


//print buffer in byte
void printBuffer(PVOID buffer, __int64 size) {
    BYTE *p = (BYTE *) buffer;
    __int64 pos = 0;
    while (pos < size) {
        fprintf(fp, "%+02X ", *(p++));
        //printf("%+02X ", *(p++));
        if (++pos % 16 == 0) { fprintf(fp, "\n"); /*printf("\n");*/}
    }
}

void printBuffer2(PVOID buffer, __int64 size) {
    BYTE *p = (BYTE *) buffer;
    __int64 pos = 0;
    while (pos < size) {
        /*fprintf(fp, "%+02X ", *(p++));*/
        printf("%+02X ", *(p++));
        if (++pos % 16 == 0) { /*fprintf(fp, "\n");*/ printf("\n"); }
    }
}

//不固定字节数的number转为带符号的INT64
INT64 Bytes2Int64(BYTE *num, UINT8 bytesCnt) {
    INT64 ret = 0;
    bool isNegative = false;
    memcpy(&ret, num, bytesCnt);
    if (ret & (1 << (bytesCnt * 8 - 1))) {
        isNegative = true;
        INT64 tmp = (INT64(0X01)) << bytesCnt * 8;
        for (int i = 0; i < 64 - bytesCnt * 8; i++) {
            ret |= (tmp);
            tmp <<= 1;
        }
    }
    return isNegative ? -(~(ret) + 1) : ret;
}

//获取物理磁盘设备号
vector<PHY_INFO> getPhyDriverNumber() {

    //这个地方应该有更好的实现方法

    vector<TCHAR> phyvols[32];
    DWORD dwSize = GetLogicalDriveStrings(0, NULL);
    char *drivers = (char *) malloc(dwSize * 2);
    DWORD dwRet = GetLogicalDriveStrings(dwSize, (LPWSTR) drivers);
    wchar_t *lp = (wchar_t *) drivers;//所有逻辑驱动器的根驱动器路径 用0隔开
    DWORD tmpNum = 0;
    while (*lp) {
        tmpNum = GetPhysicalDriveFromPartitionLetter(lp[0]);
        phyvols[tmpNum].push_back(lp[0]);
        lp += (wcslen(lp) + 1);//下一个根驱动器路径
    }
    vector<PHY_INFO> tmpPhyInfo;
    for (int i = 0; i < 32; i++) {
        if (phyvols[i].size() != 0) {
            PHY_INFO tmp;
            tmp.number = i;
            tmp.vols = phyvols[i];
            tmpPhyInfo.push_back(tmp);
        }
    }
    return tmpPhyInfo;
}

void parseMFTEntry(PVOID MFTEntry, DWORD IndexEntrySize,
                   DWORD phyDriverNumber, UINT64 volByteOffset, UINT8 secPerCluster,
                   UINT64 mftOffset, WCHAR *filePath);

void dfsIndexEntry(PVOID IndexEntryBuf, DWORD IndexEntrySize,
                   DWORD phyDriverNumber, UINT64 volByteOffset, UINT8 secPerCluster,
                   UINT64 mftOffset, WCHAR *filePath);


//DFS 索引项
//索引项 物理磁盘设备号 每个簇的扇区数 卷的物理偏移(字节) MFT的相对偏移(相对于本分区)
void dfsIndexEntry(PVOID IndexEntryBuf, DWORD IndexEntrySize,
                   DWORD phyDriverNumber, UINT64 volByteOffset, UINT8 secPerCluster, UINT64 mftOffset,
                   WCHAR *filePath) {
    printf("dfsIndexEntry\n");
    //getchar();
    //printf("IndexEntryBuf:\n");
    //printBuffer2(IndexEntryBuf, IndexEntrySize);
    //printf("\n");
    //getchar();
    pSTD_INDEX_ENTRY ptrIndexEntry = (pSTD_INDEX_ENTRY) IndexEntryBuf;
    //BYTE *ptrOfIndexEntry = (BYTE*)IndexEntryBuf;
    //获取MFT编号
    UINT64 mftReferNumber = (ptrIndexEntry->SIE_MFTReferNumber) & 0XFFFFFFFFFFFF;//取低六字节
    printf("SIE_MFTReferNumber is 0X%X\n", mftReferNumber);
    fprintf(fp, "SIE_MFTReferNumber is 0X%X\n", mftReferNumber);
    //读取文件名
    //ptrOfIndexEntry = ptrIndexEntry->SIE_FileNameAndFill;
    //UINT32 fileNameBytes = (ptrIndexEntry->SIE_FileNameSize) * 2;
    //WCHAR *fileName = (WCHAR *)malloc(fileNameBytes + 2);
    //memset(fileName, 0, fileNameBytes + 2);
    //memcpy(fileName, ptrIndexEntry->SIE_FileNameAndFill, fileNameBytes);
    //printf("SIE_FileName is %ls\n", fileName);
    //读取该索引项对应的MFT
    UINT64 mftEntryByteOffset = mftReferNumber * MFTEntrySize + mftOffset + volByteOffset;
    printf("mftOffset is %llu\n", mftOffset);
    printf("volByteOffset is %llu\n", volByteOffset);
    printf("mftEntryByteOffset is %llu\n", mftEntryByteOffset);
    PVOID mftEntryBuf = malloc(MFTEntrySize);
    ReadDisk(phyDriverNumber, mftEntryByteOffset, MFTEntrySize, mftEntryBuf);
    //printf("mftEntryBuf:\n");
    //printBuffer2(mftEntryBuf, MFTEntrySize);
    //printf("\n");
    //getchar();
    //解析MFT的0X90 0XA0属性
    parseMFTEntry(mftEntryBuf, MFTEntrySize, phyDriverNumber, volByteOffset, secPerCluster, mftOffset, filePath);
}

//解析MFT表项 获取0X90 0XA0属性
//MFT表项  物理设备号  卷物理偏移(字节)  每个簇的扇区数
void parseMFTEntry(PVOID MFTEntry, DWORD IndexEntrySize, DWORD phyDriverNumber,
                   UINT64 volByteOffset, UINT8 secPerCluster, UINT64 mftOffset, WCHAR *filePath) {
    printf("parseMFTEntry\n");
    //getchar();
    //printf("MFTEntry:\n");
    //printBuffer2(MFTEntry, MFTEntrySize);
    //printf("\n");
    //getchar();
    pFILE_RECORD_HEADER MFTEntryHeader = (pFILE_RECORD_HEADER) malloc(sizeof(FILE_RECORD_HEADER));
    memcpy(MFTEntryHeader, MFTEntry, sizeof(FILE_RECORD_HEADER));
    fprintf(fp, "MFTEntry : BytesAllocated is %u\n", MFTEntryHeader->BytesAllocated);
    fprintf(fp, "MFTEntry : BytesInUse is %u\n", MFTEntryHeader->BytesInUse);
    fprintf(fp, "MFTEntry : MFTRecordNumber is 0X%0X\n", UINT32((MFTEntryHeader->MFTRecordNumber)));
    ////
    printf("MFTEntry : BytesAllocated is %u\n", MFTEntryHeader->BytesAllocated);
    printf("MFTEntry : BytesInUse is %u\n", MFTEntryHeader->BytesInUse);
    printf("MFTEntry : MFTRecordNumber is 0X%X\n", UINT32((MFTEntryHeader->MFTRecordNumber)));
    ////
    //UINT32 MFTEntryNumber = MFTEntryHeader->MFTRecordNumber;
    //printBuffer2(MFTEntryHeader, sizeof(FILE_RECORD_HEADER));
    UINT16 attriOffset = MFTEntryHeader->AttributeOffset;//第一个属性偏移
    printf("MFTEntry : AttributeOffset is %hu\n", MFTEntryHeader->AttributeOffset);
    UINT8 *pointerInMFTEntry;
    pointerInMFTEntry = (UINT8 *) MFTEntry;
    pointerInMFTEntry += attriOffset;


    BYTE *pIndexOfMFTEntry = (BYTE *) MFTEntry;
    pIndexOfMFTEntry += attriOffset;//pIndexOfMFTEntry偏移MFT头部大小 指向第一个属性头
    UINT32 attriType, attriLen;
    //BYTE ATTR_ResFlagAttri;
    //get attribute
    pCommonAttributeHeader pComAttriHeader = (pCommonAttributeHeader) malloc(sizeof(CommonAttributeHeader));
    //存放目录下所有索引项的vector
    vector<pSTD_INDEX_ENTRY> indexEntryOfDir;
    //一个set 删除重复索引项
    map<UINT64, BOOL> visMftReferNum;
    bool finishFlag = FALSE;
    UINT32 attrCnt = 0;
    //file path
    WCHAR currentFilePath[2048] = {0};

    //MFT项的类型   00表示删除文件,01表示正常文件,02表示删除目录,03表示正常目录
    printf("isDirectoryOrFile[00删除文件,01正常文件,02删除目录,03正常目录]: %hu\n", MFTEntryHeader->Flags);
    while (TRUE) {
        //pIndexOfMFTEntry 现在指向属性头， 后面会加上这个属性的总长 指向下一个属性头

        memcpy(&attriType, pIndexOfMFTEntry, 4);//attri type
        if (attriType == 0xFFFFFFFF) {
            fprintf(fp, "\nATTR_Type is 0xFFFFFFFF, break\n");
            break;
        }
        fprintf(fp, "\nattrCnt : %u\n", attrCnt++);

        fprintf(fp, "##### AttributeHeader #####\n");

        memset(pComAttriHeader, 0, sizeof(CommonAttributeHeader));
        //属性头的通用部分
        memcpy(pComAttriHeader, pIndexOfMFTEntry, sizeof(CommonAttributeHeader));
        fprintf(fp, "ATTR_Type is 0X%X    ATTR_Size is %d\n", pComAttriHeader->ATTR_Type, pComAttriHeader->ATTR_Size);
        //如果属性总长>1024  先break
        if (pComAttriHeader->ATTR_Size > 0x400) {
            fprintf(fp, "\attriLen is more than 1024, break\n");
            break;
        }
        fprintf(fp, "ATTR_NamOff is %hu    ATTR_NamSz is %d", pComAttriHeader->ATTR_NamOff,
                pComAttriHeader->ATTR_NamSz);
        //如果当前指针偏移大于MFT已用字节数 break
        if ((pIndexOfMFTEntry - MFTEntry) > MFTEntryHeader->BytesInUse) {
            fprintf(fp, "\nreach end of BytesInUse, break\n");
            break;
        }

        //resolve attribute header
        UINT16 attriHeaderSize = 0;
        bool isResidentAttri = false;
        switch (pComAttriHeader->ATTR_ResFlag)//是否常驻属性
        {
            case BYTE(0x00): {
                isResidentAttri = true;
                //get attribute header
                pResidentAttributeHeader residentAttriHeader = (pResidentAttributeHeader) malloc(
                        sizeof(ResidentAttributeHeader));
                memcpy(residentAttriHeader, pIndexOfMFTEntry, sizeof(ResidentAttributeHeader));
                fprintf(fp, "\n\n常驻属性\n\n");
                //fprintf(fp, "ATTR_Size is %u\n", residentAttriHeader->ATTR_Size);
                fprintf(fp, "ATTR_DatOff[属性头长度] is %hu\n", residentAttriHeader->ATTR_DatOff);
                attriHeaderSize = residentAttriHeader->ATTR_DatOff;
                UINT16 ResidentAttributeHeaderSize = residentAttriHeader->ATTR_DatOff;
                residentAttriHeader = (pResidentAttributeHeader) realloc(residentAttriHeader,
                                                                         ResidentAttributeHeaderSize);
                memcpy(residentAttriHeader, pIndexOfMFTEntry, ResidentAttributeHeaderSize);
                //fprintf(fp, "ATTR_AttrNam[属性名] is %ls\n", residentAttriHeader->ATTR_AttrNam);
                fprintf(fp, "ATTR_DatSz[属性体长度] is %u\n", residentAttriHeader->ATTR_DatSz);
                fprintf(fp, "ATTR_Indx[属性索引] is %u\n", residentAttriHeader->ATTR_Indx);

                break;
            }
            case BYTE(0x01): {
                isResidentAttri = false;
                //get attribute header
                fprintf(fp, "\n\n非常驻属性\n\n");
                pNonResidentAttributeHeader nonResidentAttriHeader = (pNonResidentAttributeHeader) malloc(
                        sizeof(NonResidentAttributeHeader));
                memcpy(nonResidentAttriHeader, pIndexOfMFTEntry, sizeof(NonResidentAttributeHeader));
                //fprintf(fp, "\n\n非常驻属性\nATTR_Type is 0x%X\n", nonResidentAttriHeader->ATTR_Type);
                fprintf(fp, "ATTR_DatOff[属性头长度] is %hu\n", nonResidentAttriHeader->ATTR_DatOff);
                attriHeaderSize = nonResidentAttriHeader->ATTR_DatOff;
                UINT16 NonResidentAttributeHeaderSize = nonResidentAttriHeader->ATTR_DatOff;
                nonResidentAttriHeader = (pNonResidentAttributeHeader) realloc(nonResidentAttriHeader,
                                                                               NonResidentAttributeHeaderSize);
                memcpy(nonResidentAttriHeader, pIndexOfMFTEntry, NonResidentAttributeHeaderSize);
                fprintf(fp, "ATTR_StartVCN[起始VCN] is %llu\n", nonResidentAttriHeader->ATTR_StartVCN);
                fprintf(fp, "ATTR_EndVCN[终止VCN] is %llu\n", nonResidentAttriHeader->ATTR_EndVCN);
                fprintf(fp, "ATTR_ValidSz[属性实际长度 is %llu\n", nonResidentAttriHeader->ATTR_ValidSz);
                fprintf(fp, "ATTR_AllocSz[属性分配长度] is %llu\n", nonResidentAttriHeader->ATTR_AllocSz);
                fprintf(fp, "ATTR_InitedSz[属性初始长度] is %llu\n", nonResidentAttriHeader->ATTR_InitedSz);
                //fprintf(fp, "ATTR_AttrNam[属性名] is %ls\n", nonResidentAttriHeader->ATTR_AttrNam);

                break;
            }
            default:

                break;
        }

        fprintf(fp, "\n##### END of AttributeHeader #####\n");

        //resolve attribute data
        BYTE *tmpAttriDataIndex = pIndexOfMFTEntry;
        //待修改
        //tmpAttriDataIndex += (pComAttriHeader->ATTR_ResFlag == BYTE(0x00) ? sizeof(ResidentAttributeHeader) : sizeof(NonResidentAttributeHeader));
        tmpAttriDataIndex += (attriHeaderSize);

        //tmpAttriDataIndex指向属性体
        switch (pComAttriHeader->ATTR_Type) {
            case 0x00000030: {
                //文件名属性 可能多个

                pFILE_NAME ptrFileName = (pFILE_NAME) malloc(sizeof(FILE_NAME));
                memcpy(ptrFileName, tmpAttriDataIndex, sizeof(FILE_NAME));
                if (ptrFileName->FN_NamSpace == 0X02) { break; }
                fprintf(fp, "\n##### FILE_NAME #####\n");
                printf("\n##### FILE_NAME #####\n");
                fprintf(fp, "FN_NameSz is %d\n", ptrFileName->FN_NameSz);
                printf("FN_NameSz is %d\n", ptrFileName->FN_NameSz);
                fprintf(fp, "FN_NamSpace is %d\n", ptrFileName->FN_NamSpace);
                printf("FN_NamSpace is %d\n", ptrFileName->FN_NamSpace);
                //get file name
                UINT32 fileNameLen = UINT32(0xFFFF & (ptrFileName->FN_NameSz) + 1) << 1;
                WCHAR *fileName = (WCHAR *) malloc(fileNameLen);
                memset(fileName, 0, fileNameLen);
                memcpy(fileName, tmpAttriDataIndex + sizeof(FILE_NAME), fileNameLen - 2);
                //printf("FILENAME[0X] is:\n");
                //printBuffer2(fileName, fileNameLen);
                //printf("\n");
                fprintf(fp, "FILENAME is %ls\n", fileName);
                printf("FILENAME is %ls\n", fileName);

                memset(currentFilePath, 0, sizeof(currentFilePath));
                wcscpy(currentFilePath, filePath);
                wcscat(currentFilePath, L"\\");
                wcscat(currentFilePath, fileName);

                printf("\n\n------>\ncurrentFilePath is %ls\n", currentFilePath);
                fprintf(fp, "---> currentFilePath is %ls\n", currentFilePath);

                fprintf(fp, "\n##### END of FILE_NAME #####\n");
                printf("\n##### END of FILE_NAME #####\n");
                getchar();
                break;
            }
            case 0x00000090: {//INDEX_ROOT 索引根
                pINDEX_ROOT pIndexRoot = (INDEX_ROOT *) malloc(sizeof(INDEX_ROOT));
                memcpy(pIndexRoot, tmpAttriDataIndex, sizeof(INDEX_ROOT));

                fprintf(fp, "\n##### INDEX_ROOT #####\n");
                fprintf(fp, "IR_EntrySz[目录项的大小,一般是一个簇] is %u\n", pIndexRoot->IR_EntrySz);
                fprintf(fp, "IR_ClusPerRec[目录项占用的簇数,一般是一个] is %u\n", pIndexRoot->IR_ClusPerRec);
                fprintf(fp, "IH_TalSzOfEntries[索引根和紧随其后的索引项的大小] is %u\n", pIndexRoot->IH.IH_TalSzOfEntries);
                fprintf(fp, "IH_EntryOff[第一个索引项的偏移] is %u\n", pIndexRoot->IH.IH_EntryOff);

                printf("\n##### INDEX_ROOT #####\n");
                printf("IR_EntrySz[目录项的大小,一般是一个簇] is %u\n", pIndexRoot->IR_EntrySz);
                printf("IR_ClusPerRec[目录项占用的簇数,一般是一个] is %u\n", pIndexRoot->IR_ClusPerRec);
                printf("IH_TalSzOfEntries[索引根和紧随其后的索引项的大小] is %u\n", pIndexRoot->IH.IH_TalSzOfEntries);
                printf("IH_EntryOff[第一个索引项的偏移] is %u\n", pIndexRoot->IH.IH_EntryOff);
                //0X90属性的实际大小
                UINT32 attri90Size = sizeof(INDEX_ROOT) - sizeof(INDEX_HEADER) + pIndexRoot->IH.IH_TalSzOfEntries;
                pIndexRoot = (INDEX_ROOT *) realloc(pIndexRoot, attri90Size);
                memcpy(pIndexRoot, tmpAttriDataIndex, attri90Size);
                //获取90属性中的索引头
                INDEX_HEADER IR_IH = pIndexRoot->IH;
                UINT32 indexTotalSize = pIndexRoot->IH.IH_TalSzOfEntries;//索引头和接下来的索引项的总大小 注意可能没有索引项
                //获取90属性中的索引项
                BYTE *pIndexOfEntry, *ptrIndexHeaderStart;//索引项的指针 索引头的指针
                pIndexOfEntry = pIndexRoot->IR_IndexEntry;
                ptrIndexHeaderStart = (BYTE *) (&(pIndexRoot->IH));
                UINT32 indexEntryIn90AttriCnt = 0;
                while (TRUE) {
                    UINT64 isIndexEntryFinish = 0;
                    memcpy(&isIndexEntryFinish, pIndexOfEntry, 8);
                    if (isIndexEntryFinish == 0X00) {
                        //MFT号是0 break
                        break;
                    }
                    if (pIndexOfEntry - ptrIndexHeaderStart > indexTotalSize) {
                        //超出有效长度 break
                        break;
                    }

                    INDEX_ENTRY *pIndexEntry = (INDEX_ENTRY *) pIndexOfEntry;
                    //printf("IE_FileNameSize is %d\n", pIndexEntry->IE_FileNameSize);
                    UINT32 fileNameBytes = (pIndexEntry->IE_FileNameSize) * 2;
                    WCHAR *fileName = (WCHAR *) malloc(fileNameBytes + 2);
                    memset(fileName, 0, fileNameBytes + 2);
                    memcpy(fileName, pIndexEntry->IE_FileNameAndFill, fileNameBytes);
                    //printf("IE_FileName is %ls\n", fileName);
                    indexEntryIn90AttriCnt++;
                    //printBuffer2(pIndexEntry, pIndexEntry->IE_Size);
                    //printf("\n");
                    //getchar();
                    /*******************************/
                    if (visMftReferNum.find((pIndexEntry->IE_MftReferNumber) & 0XFFFFFFFFFFFF) ==
                        visMftReferNum.end() && ((((pIndexEntry->IE_MftReferNumber) >> (8 * 6)) & 0XFFFF) != 0X00)) {
                        // $ObjId的索引项正常  但其指向的MFT的90属性异常  其中该MFT号的高2位为0  90属性中的MFT引用号异常
                        //这里读到了索引项
                        printf("find index entry in 90 attribute, cnt is %d\n", indexEntryIn90AttriCnt);
                        fprintf(fp, "find index entry in 90 attribute, cnt is %d\n", indexEntryIn90AttriCnt);
                        indexEntryOfDir.push_back(pSTD_INDEX_ENTRY(pIndexEntry));
                        visMftReferNum.insert(
                                pair<UINT64, BOOL>((pIndexEntry->IE_MftReferNumber) & 0XFFFFFFFFFFFF, TRUE));
                    }
                    //dfsIndexEntry((PVOID)pIndexEntry, pIndexEntry->IE_Size, phyDriverNumber, volByteOffset, secPerCluster, mftOffset);
                    /*******************************/
                    pIndexOfEntry += pIndexEntry->IE_Size;
                    //getchar();
                }
                printf("\n##### END of INDEX_ROOT #####\n");

                break;
            }
            case 0x000000A0: {//INDEX_ALLOCATION

                fprintf(fp, "\n##### INDEX_ALLOCATION #####\n");
                printf("\n##### INDEX_ALLOCATION #####\n");
                //A0属性体

                //存放VCN LCN
                vector<VCN_LCN_SIZE> dataRuns;
                //data runs 起始指针
                BYTE *dataRunsStartOffset = tmpAttriDataIndex;
                //data runs 总字节数
                UINT32 attriBodySize = (pIndexOfMFTEntry + pComAttriHeader->ATTR_Size) - tmpAttriDataIndex;

                //printf("attriBodySize is %u\n", attriBodySize);
                //getchar();
                UINT64 vcnCnt = 0;
                while ((*dataRunsStartOffset) != 0X00 && dataRunsStartOffset - tmpAttriDataIndex < attriBodySize) {
                    UINT8 bytesClustersOfStdIndex = (*dataRunsStartOffset) & 0X0F, bytesCluOffsetOfStdIndex =
                            (*dataRunsStartOffset >> 4) & 0X0F;
                    UINT32 totalBytes = bytesClustersOfStdIndex + bytesCluOffsetOfStdIndex;
                    //VCN.push_back(vcnCnt++);
                    UINT64 clustersOfStdIndex = 0;
                    INT64 cluOffsetOfStdIndex = 0;
                    //BYTE *ptrInDataRuns = (BYTE *)(&dataRuns);

                    memcpy(&clustersOfStdIndex, dataRunsStartOffset + 1, bytesClustersOfStdIndex);
                    //memcpy(&cluOffsetOfStdIndex, dataRunsStartOffset + 1 + bytesClustersOfStdIndex, bytesCluOffsetOfStdIndex);
                    cluOffsetOfStdIndex = Bytes2Int64(dataRunsStartOffset + 1 + bytesClustersOfStdIndex,
                                                      bytesCluOffsetOfStdIndex);
                    //printf("Bytes2Int64 is %lld\n",Bytes2Int64(dataRunsStartOffset + 1 + bytesClustersOfStdIndex, bytesCluOffsetOfStdIndex));

                    if (vcnCnt == 0) {
                        dataRuns.push_back(VCN_LCN_SIZE(vcnCnt, cluOffsetOfStdIndex, clustersOfStdIndex));
                    } else {
                        dataRuns.push_back(VCN_LCN_SIZE(vcnCnt, dataRuns[vcnCnt - 1].LCN + cluOffsetOfStdIndex,
                                                        clustersOfStdIndex));
                    }
                    vcnCnt++;
                    dataRunsStartOffset += (totalBytes + 1);
                }

                printf("-->INDEX_ALLOCATION Cluster Info\n");
                for (int i = 0; i < dataRuns.size(); i++) {
                    printf("VCN, LCN, SIZE: %llu, %llu, %llu\n", dataRuns[i].VCN, dataRuns[i].LCN, dataRuns[i].SIZE);
                }
                printf("-->End of INDEX_ALLOCATION Cluster Info\n");
                getchar();

                for (int i = 0; i < dataRuns.size(); i++) {
                    DWORD stdIndexSize = dataRuns[i].SIZE * secPerCluster * SectorSize;//标准索引占用的总字节数
                    UINT64 stdIndexByteOffset = volByteOffset + dataRuns[i].LCN * secPerCluster * SectorSize;//标准索引的物理偏移
                    //读取标准索引区的数据 该data run 的N个簇全部读取
                    PVOID pStdIndexBuffer = malloc(stdIndexSize);
                    ReadDisk(phyDriverNumber, stdIndexByteOffset, stdIndexSize, pStdIndexBuffer);
                    fprintf(fp, "->data run cnt: %d\n", i);
                    printf("->data run cnt: %d\n", i);

                    UINT32 indexClusterCnt = 0;
                    //逐个簇分析
                    UINT32 indexEntryInIndxArea = 0;
                    while (indexClusterCnt < dataRuns[i].SIZE) {
                        //
                        fprintf(fp, "->cluster cnt: %d\n", indexClusterCnt);
                        printf("->cluster cnt: %d\n", indexClusterCnt);
                        //读取标准索引的头部
                        BYTE *ptrOfStdIndexBuffer =
                                (BYTE *) pStdIndexBuffer + (indexClusterCnt * secPerCluster * SectorSize);//标准索引的指针
                        indexClusterCnt++;
                        //ptrIndex指向该索引簇的起始位置
                        BYTE *ptrIndex = ptrOfStdIndexBuffer;
                        pSTD_INDEX_HEADER pStdIndexHeader = (pSTD_INDEX_HEADER) malloc(sizeof(STD_INDEX_HEADER));
                        memcpy(pStdIndexHeader, ptrOfStdIndexBuffer, sizeof(STD_INDEX_HEADER));
                        UINT32 SIH_Flag;
                        memcpy(&SIH_Flag, pStdIndexHeader->SIH_Flag, 4);
                        if (SIH_Flag == 0X00000000) { break; }
                        //标准索引头的大小=第一个索引项的偏移+24字节
                        UINT32 stdIndexHeaderSize = pStdIndexHeader->SIH_IndexEntryOffset + 8 * 3;
                        pStdIndexHeader = (pSTD_INDEX_HEADER) realloc(pStdIndexHeader, stdIndexHeaderSize);
                        memcpy(pStdIndexHeader, ptrOfStdIndexBuffer, stdIndexHeaderSize);
                        //print
                        fprintf(fp, "\n##### STD_INDEX_HEADER #####\n");
                        fprintf(fp, "SIH_IndexEntryOffset[索引项偏移,从此位置开始] is %u\n",
                                pStdIndexHeader->SIH_IndexEntryOffset);
                        fprintf(fp, "SIH_IndexEntrySize[索引项总大小] is %u\n", pStdIndexHeader->SIH_IndexEntrySize);
                        printf("\n##### STD_INDEX_HEADER #####\n");
                        printf("SIH_IndexEntryOffset[索引项偏移,从此位置开始] is %u\n", pStdIndexHeader->SIH_IndexEntryOffset);
                        printf("SIH_IndexEntrySize[索引项总大小] is %u\n", pStdIndexHeader->SIH_IndexEntrySize);
                        UINT32 stdIndexTotalSize = pStdIndexHeader->SIH_IndexEntrySize;
                        printf("SIH_IndexEntryAllocSize[索引项总分配大小] is %u\n", pStdIndexHeader->SIH_IndexEntryAllocSize);
                        printf("\n##### END of STD_INDEX_HEADER #####\n");
                        fprintf(fp, "SIH_IndexEntryAllocSize[索引项总分配大小] is %u\n",
                                pStdIndexHeader->SIH_IndexEntryAllocSize);
                        fprintf(fp, "\n##### END of STD_INDEX_HEADER #####\n");
                        //指针移动到索引项
                        ptrOfStdIndexBuffer += stdIndexHeaderSize;
                        //BYTE *ptrIndexEntryStartOffset = ptrOfStdIndexBuffer;
                        while (TRUE) {
                            //MFT编号为0 break
                            UINT64 isIndexEntryFinish = 0;
                            memcpy(&isIndexEntryFinish, ptrOfStdIndexBuffer, 8);
                            //超出有效长度 break
                            if (ptrOfStdIndexBuffer - ptrIndex > stdIndexTotalSize) {
                                fprintf(fp, "超出INDEX有效范围, break\n");
                                printf("超出INDEX有效范围, break\n");
                                break;
                            }
                            //if (isIndexEntryFinish == UINT64(0)) { break; }
                            if (isIndexEntryFinish == UINT64(0)) { fprintf(fp, "find mft refer number[00]\n"); }
                            if (isIndexEntryFinish == UINT64(0)) { printf("find mft refer number[00]\n"); }
                            //printf("ptrOfStdIndexBuffer - ptrIndexEntryStartOffset is %d\n", ptrOfStdIndexBuffer - ptrIndexEntryStartOffset);
                            //逐个读取索引项
                            pSTD_INDEX_ENTRY pStdIndexEntry = (pSTD_INDEX_ENTRY) malloc(sizeof(STD_INDEX_ENTRY));
                            memcpy(pStdIndexEntry, ptrOfStdIndexBuffer, sizeof(STD_INDEX_ENTRY));
                            UINT32 stdIndexEntrySize = pStdIndexEntry->SIE_IndexEntrySize;
                            pStdIndexEntry = (pSTD_INDEX_ENTRY) realloc(pStdIndexEntry, stdIndexEntrySize);
                            memcpy(pStdIndexEntry, ptrOfStdIndexBuffer, stdIndexEntrySize);
                            if (pStdIndexEntry->SIE_MFTReferNumber == UINT64(0)) { break; }
                            //printf("\n##### STD_INDEX_ENTRY #####\n");
                            //fprintf(fp, "\n");
                            //printBuffer(pStdIndexEntry, stdIndexEntrySize);
                            //fprintf(fp, "\n");
                            //printf("SIE_MFTReferNumber is %d\n", (pStdIndexEntry->SIE_MFTReferNumber) & 0XFFFFFFFFFFFF);
                            //printf("SIE_IndexEntrySize[索引项大小] is %u\n", (pStdIndexEntry->SIE_IndexEntrySize));
                            //printf("SIE_FileNameSize is %d\n", (pStdIndexEntry->SIE_FileNameSize));
                            //printf("SIE_FileAllocSize is %llu\n", (pStdIndexEntry->SIE_FileAllocSize));
                            //printf("SIE_FileRealSize is %llu\n", (pStdIndexEntry->SIE_FileRealSize));
                            //UINT8 fileNameBytes = (pStdIndexEntry->SIE_FileNameSize) * 2;
                            //WCHAR *fileName = (WCHAR *)malloc(fileNameBytes + 2);
                            //memcpy(fileName, pStdIndexEntry->SIE_FileNameAndFill, fileNameBytes);
                            //printf("SIE_FileName is %ls\n", fileName);
                            //printf("\n##### END of STD_INDEX_ENTRY #####\n");
                            //这里读到了索引项
                            /********************************/
                            if (visMftReferNum.find((pStdIndexEntry->SIE_MFTReferNumber) & 0XFFFFFFFFFFFF) ==
                                visMftReferNum.end() &&
                                ((((pStdIndexEntry->SIE_MFTReferNumber) >> (8 * 6)) & 0XFFFF) != 0X00)) {
                                printf("find index entry in INDX area, cnt is %d\n", indexEntryInIndxArea);
                                fprintf(fp, "find index entry in INDX area, cnt is %d\n", indexEntryInIndxArea);
                                indexEntryInIndxArea++;
                                indexEntryOfDir.push_back(pSTD_INDEX_ENTRY(pStdIndexEntry));
                                visMftReferNum.insert(
                                        pair<UINT64, BOOL>((pStdIndexEntry->SIE_MFTReferNumber) & 0XFFFFFFFFFFFF,
                                                           TRUE));
                            }
                            //dfsIndexEntry((PVOID)pStdIndexEntry, stdIndexEntrySize, phyDriverNumber, volByteOffset, secPerCluster, mftOffset);
                            /********************************/
                            //ptrOfStdIndexBuffer指向下一个索引项
                            ptrOfStdIndexBuffer += stdIndexEntrySize;
                            //getchar();
                        }

                        //printBuffer2(pStdIndexBuffer, stdIndexSize);
                        //getchar();
                        //fprintf(fp, "\n##### END of INDEX_ALLOCATION #####\n");
                        //printf("\n##### END of INDEX_ALLOCATION #####\n");
                    }


                }

                fprintf(fp, "\n##### END of INDEX_ALLOCATION #####\n");
                printf("\n##### END of INDEX_ALLOCATION #####\n");

                break;
            }
            case 0x00000010: {
                break;
            }
            case 0x00000020: {
                break;
            }
            case 0x00000040: {
                break;
            }
            case 0x00000050: {
                break;
            }
            case 0x00000060: {
                break;
            }
            case 0x00000070: {
                break;
            }
            case 0x00000080: {
                //文件数据属性

                //80属性体长度
                UINT32 attriBodySize = (pIndexOfMFTEntry + pComAttriHeader->ATTR_Size) - tmpAttriDataIndex;
                //属性体
                pDATA ptrData = (pDATA) malloc(attriBodySize);
                memcpy(ptrData, tmpAttriDataIndex, attriBodySize);
                //如果是常驻属性  80属性体就是文件内容
                if (isResidentAttri) {
                    //80属性体中的文件内容
                    printf("content in 0X80[resident]:\n");
                    printBuffer2(ptrData, attriBodySize);
                    printf("\n");
                }
                    //如果是非常驻属性 80属性体是data runs
                else {
                    printf("content in 0X80[nonresident]:\n");
                    printBuffer2(ptrData, attriBodySize);
                    printf("\n");
                    //存放VCN LCN
                    vector<VCN_LCN_SIZE> dataRuns;
                    //data runs 起始指针
                    BYTE *dataRunsStartOffset = tmpAttriDataIndex;
                    //data runs 总字节数
                    UINT32 attriBodySize = (pIndexOfMFTEntry + pComAttriHeader->ATTR_Size) - tmpAttriDataIndex;

                    //printf("attriBodySize is %u\n", attriBodySize);
                    //getchar();
                    UINT64 vcnCnt = 0;
                    while ((*dataRunsStartOffset) != 0X00 && dataRunsStartOffset - tmpAttriDataIndex < attriBodySize) {
                        UINT8 bytesClustersOfStdIndex = (*dataRunsStartOffset) & 0X0F, bytesCluOffsetOfStdIndex =
                                (*dataRunsStartOffset >> 4) & 0X0F;
                        UINT32 totalBytes = bytesClustersOfStdIndex + bytesCluOffsetOfStdIndex;
                        //VCN.push_back(vcnCnt++);
                        UINT64 clustersOfStdIndex = 0;
                        INT64 cluOffsetOfStdIndex = 0;
                        //BYTE *ptrInDataRuns = (BYTE *)(&dataRuns);

                        memcpy(&clustersOfStdIndex, dataRunsStartOffset + 1, bytesClustersOfStdIndex);
                        //memcpy(&cluOffsetOfStdIndex, dataRunsStartOffset + 1 + bytesClustersOfStdIndex, bytesCluOffsetOfStdIndex);
                        cluOffsetOfStdIndex = Bytes2Int64(dataRunsStartOffset + 1 + bytesClustersOfStdIndex,
                                                          bytesCluOffsetOfStdIndex);
                        //printf("Bytes2Int64 is %lld\n",Bytes2Int64(dataRunsStartOffset + 1 + bytesClustersOfStdIndex, bytesCluOffsetOfStdIndex));

                        if (vcnCnt == 0) {
                            dataRuns.push_back(VCN_LCN_SIZE(vcnCnt, cluOffsetOfStdIndex, clustersOfStdIndex));
                        } else {
                            dataRuns.push_back(VCN_LCN_SIZE(vcnCnt, dataRuns[vcnCnt - 1].LCN + cluOffsetOfStdIndex,
                                                            clustersOfStdIndex));
                        }
                        vcnCnt++;
                        dataRunsStartOffset += (totalBytes + 1);
                    }

                    printf("-->DATA Cluster Info\n");
                    for (int i = 0; i < dataRuns.size(); i++) {
                        printf("VCN, LCN, SIZE: %llu, %llu, %llu\n", dataRuns[i].VCN, dataRuns[i].LCN,
                               dataRuns[i].SIZE);
                    }
                    //拿到了文件每一块的LCN和对应的簇数
                    //能够直接读取文件内容了

                    printf("-->End of DATA Cluster Info\n");
                    getchar();
                }


                break;
            }
            case 0x000000B0: {
                break;
            }
            case 0x000000C0: {
                break;
            }
            case 0x000000D0: {
                break;
            }
            case 0x000000E0: {
                break;
            }
            case 0x000000F0: {
                break;
            }
            case 0x00000100: {
                break;
            }
            default: {
                finishFlag = TRUE;
                break;
            }
        }
        pIndexOfMFTEntry += pComAttriHeader->ATTR_Size;
        if (finishFlag) { break; }
    }
    for (int i = 0; i < indexEntryOfDir.size(); i++) {
        //
        fprintf(fp, "WATCH :: IndexEntryOfDir -> %d:\n", i);
        printBuffer(indexEntryOfDir[i], indexEntryOfDir[i]->SIE_IndexEntrySize);
        fprintf(fp, "\n");
        //printf("WATCH :: IndexEntryOfDir -> %d:\n", i);
        //printBuffer2(indexEntryOfDir[i], indexEntryOfDir[i]->SIE_IndexEntrySize);
        //printf("\n");
        //getchar();
        //dfsIndexEntry((PVOID)(indexEntryOfDir[i]), indexEntryOfDir[i]->SIE_IndexEntrySize, phyDriverNumber, volByteOffset, secPerCluster, mftOffset);
    }
    for (int i = 0; i < indexEntryOfDir.size(); i++) {
        //
        //printf("indexEntryOfDir of root:\n");
        //printBuffer2(indexEntryOfDir[i], indexEntryOfDir[i]->SIE_IndexEntrySize);
        //printf("\n");
        //getchar();
        fprintf(fp, "begin DFS in Dir\n");
        fprintf(fp, "Index Entry Buff:\n");
        printBuffer(indexEntryOfDir[i], indexEntryOfDir[i]->SIE_IndexEntrySize);
        fprintf(fp, "\n");
        printf("begin DFS in Dir\n");
        //printf("Index Entry Buff:\n");
        //printBuffer2(indexEntryOfDir[i], indexEntryOfDir[i]->SIE_IndexEntrySize);
        printf("\n");
        if (UINT64((indexEntryOfDir[i]->SIE_MFTReferNumber) & 0XFFFFFFFFFFFF) < 16) {
            fprintf(fp, "SIE_MFTReferNumber < 16, continue\n");
            //printf("SIE_MFTReferNumber < 16, continue\n");
        } else {
            fprintf(fp, "SIE_MFTReferNumber  >= 16, begin search\n");
            //printf("SIE_MFTReferNumber  >= 16, begin search\n");
            dfsIndexEntry((PVOID) (indexEntryOfDir[i]), indexEntryOfDir[i]->SIE_IndexEntrySize, phyDriverNumber,
                          volByteOffset, secPerCluster, mftOffset, currentFilePath);
        }
        //dfsIndexEntry((PVOID)(indexEntryOfDir[i]), indexEntryOfDir[i]->SIE_IndexEntrySize, phyDriverNumber, volByteOffset, secPerCluster, mftOffset);
    }
    indexEntryOfDir.clear();
    visMftReferNum.clear();
    //getchar();
}


//解析MFT表项
void resolveMFTEntry(PVOID MFTEntry, DWORD phyDriverNumber, UINT64 volByteOffset, UINT8 secPerCluster,
                     UINT64 mftOffset) {//MFT表项  物理设备号  卷物理偏移(字节)  每个簇的扇区数 mft相对于该分区的偏移
    //printBuffer(MFTEntry, MFTEntrySize);
    //get MFTEntryHeader
    pFILE_RECORD_HEADER MFTEntryHeader = (pFILE_RECORD_HEADER) malloc(sizeof(FILE_RECORD_HEADER));
    memcpy(MFTEntryHeader, MFTEntry, sizeof(FILE_RECORD_HEADER));
    fprintf(fp, "MFTEntry : BytesAllocated is %u\n", MFTEntryHeader->BytesAllocated);
    fprintf(fp, "MFTEntry : BytesInUse is %u\n", MFTEntryHeader->BytesInUse);
    fprintf(fp, "MFTEntry : MFTRecordNumber is %u\n", MFTEntryHeader->MFTRecordNumber);
    UINT32 MFTEntryNumber = MFTEntryHeader->MFTRecordNumber;
    //printBuffer2(MFTEntryHeader, sizeof(FILE_RECORD_HEADER));
    UINT16 attriOffset = MFTEntryHeader->AttributeOffset;//第一个属性偏移
    UINT8 *pointerInMFTEntry;
    pointerInMFTEntry = (UINT8 *) MFTEntry;
    pointerInMFTEntry += attriOffset;


    BYTE *pIndexOfMFTEntry = (BYTE *) MFTEntry;
    pIndexOfMFTEntry += attriOffset;//pIndexOfMFTEntry偏移MFT头部大小 指向第一个属性头
    UINT32 attriType, attriLen;
    //BYTE ATTR_ResFlagAttri;
    //get attribute
    pCommonAttributeHeader pComAttriHeader = (pCommonAttributeHeader) malloc(sizeof(CommonAttributeHeader));
    bool finishFlag = FALSE;
    UINT32 attrCnt = 0;

    while (TRUE) {
        //pIndexOfMFTEntry 现在指向属性头， 后面会加上这个属性的总长 指向下一个属性头

        memcpy(&attriType, pIndexOfMFTEntry, 4);//attri type
        if (attriType == 0xFFFFFFFF) {
            fprintf(fp, "\nATTR_Type is 0xFFFFFFFF, break\n");
            break;
        }
        fprintf(fp, "\nattrCnt : %u\n", attrCnt++);

        fprintf(fp, "##### AttributeHeader #####\n");

        memset(pComAttriHeader, 0, sizeof(CommonAttributeHeader));
        //属性头的通用部分
        memcpy(pComAttriHeader, pIndexOfMFTEntry, sizeof(CommonAttributeHeader));
        fprintf(fp, "ATTR_Type is 0X%X    ATTR_Size is %d\n", pComAttriHeader->ATTR_Type, pComAttriHeader->ATTR_Size);
        //如果属性总长>1024  先break
        if (pComAttriHeader->ATTR_Size > 0x400) {
            fprintf(fp, "\attriLen is more than 1024, break\n");
            break;
        }
        fprintf(fp, "ATTR_NamOff is %hu    ATTR_NamSz is %d", pComAttriHeader->ATTR_NamOff,
                pComAttriHeader->ATTR_NamSz);
        //如果当前指针偏移大于MFT已用字节数 break
        if ((pIndexOfMFTEntry - MFTEntry) > MFTEntryHeader->BytesInUse) {
            fprintf(fp, "\nreach end of BytesInUse, break\n");
            break;
        }
        //resolve attribute header
        UINT16 attriHeaderSize = 0;
        switch (pComAttriHeader->ATTR_ResFlag)//是否常驻属性
        {
            case BYTE(0x00): {

                //get attribute header
                pResidentAttributeHeader residentAttriHeader = (pResidentAttributeHeader) malloc(
                        sizeof(ResidentAttributeHeader));
                memcpy(residentAttriHeader, pIndexOfMFTEntry, sizeof(ResidentAttributeHeader));
                fprintf(fp, "\n\n常驻属性\n\n");
                //fprintf(fp, "ATTR_Size is %u\n", residentAttriHeader->ATTR_Size);
                fprintf(fp, "ATTR_DatOff[属性头长度] is %hu\n", residentAttriHeader->ATTR_DatOff);
                attriHeaderSize = residentAttriHeader->ATTR_DatOff;
                UINT16 ResidentAttributeHeaderSize = residentAttriHeader->ATTR_DatOff;
                residentAttriHeader = (pResidentAttributeHeader) realloc(residentAttriHeader,
                                                                         ResidentAttributeHeaderSize);
                memcpy(residentAttriHeader, pIndexOfMFTEntry, ResidentAttributeHeaderSize);
                //fprintf(fp, "ATTR_AttrNam[属性名] is %ls\n", residentAttriHeader->ATTR_AttrNam);
                fprintf(fp, "ATTR_DatSz[属性体长度] is %u\n", residentAttriHeader->ATTR_DatSz);
                fprintf(fp, "ATTR_Indx[属性索引] is %u\n", residentAttriHeader->ATTR_Indx);

                break;
            }
            case BYTE(0x01): {
                //get attribute header
                fprintf(fp, "\n\n非常驻属性\n\n");
                pNonResidentAttributeHeader nonResidentAttriHeader = (pNonResidentAttributeHeader) malloc(
                        sizeof(NonResidentAttributeHeader));
                memcpy(nonResidentAttriHeader, pIndexOfMFTEntry, sizeof(NonResidentAttributeHeader));
                //fprintf(fp, "\n\n非常驻属性\nATTR_Type is 0x%X\n", nonResidentAttriHeader->ATTR_Type);
                fprintf(fp, "ATTR_DatOff[属性头长度] is %hu\n", nonResidentAttriHeader->ATTR_DatOff);
                attriHeaderSize = nonResidentAttriHeader->ATTR_DatOff;
                UINT16 NonResidentAttributeHeaderSize = nonResidentAttriHeader->ATTR_DatOff;
                nonResidentAttriHeader = (pNonResidentAttributeHeader) realloc(nonResidentAttriHeader,
                                                                               NonResidentAttributeHeaderSize);
                memcpy(nonResidentAttriHeader, pIndexOfMFTEntry, NonResidentAttributeHeaderSize);
                fprintf(fp, "ATTR_StartVCN[起始VCN] is %llu\n", nonResidentAttriHeader->ATTR_StartVCN);
                fprintf(fp, "ATTR_EndVCN[终止VCN] is %llu\n", nonResidentAttriHeader->ATTR_EndVCN);
                fprintf(fp, "ATTR_ValidSz[属性实际长度 is %llu\n", nonResidentAttriHeader->ATTR_ValidSz);
                fprintf(fp, "ATTR_AllocSz[属性分配长度] is %llu\n", nonResidentAttriHeader->ATTR_AllocSz);
                fprintf(fp, "ATTR_InitedSz[属性初始长度] is %llu\n", nonResidentAttriHeader->ATTR_InitedSz);
                //fprintf(fp, "ATTR_AttrNam[属性名] is %ls\n", nonResidentAttriHeader->ATTR_AttrNam);

                break;
            }
            default:

                break;
        }

        fprintf(fp, "\n##### END of AttributeHeader #####\n");

        //resolve attribute data
        BYTE *tmpAttriDataIndex = pIndexOfMFTEntry;
        //指向属性体
        tmpAttriDataIndex += (attriHeaderSize);
        //tmpAttriDataIndex指向属性体
        switch (pComAttriHeader->ATTR_Type) {
            case 0x00000010: {//STANDARD_INFORMATION  10属性

                pSTANDARD_INFORMATION stdInfo = (pSTANDARD_INFORMATION) malloc(sizeof(STANDARD_INFORMATION));
                memcpy(stdInfo, tmpAttriDataIndex, sizeof(STANDARD_INFORMATION));
                SYSTEMTIME sysTime;
                FileTimeToSystemTime(&(stdInfo->SI_CreatTime), &sysTime);
                fprintf(fp, "\n##### STANDARD_INFORMATION #####\n");
                //printBuffer(stdInfo, sizeof(STANDARD_INFORMATION));
                fprintf(fp, "\n");
                fprintf(fp, "SI_CreatTime-wYear is %hu\n", sysTime.wYear);
                fprintf(fp, "SI_CreatTime-wMonth is %hu\n", sysTime.wMonth);
                fprintf(fp, "SI_CreatTime-wDay is %hu\n", sysTime.wDay);
                fprintf(fp, "SI_CreatTime-wHour is %hu\n", sysTime.wHour);
                fprintf(fp, "SI_CreatTime-wMinute is %hu\n", sysTime.wMinute);
                fprintf(fp, "SI_CreatTime-wSecond is %hu\n", sysTime.wSecond);
                fprintf(fp, "SI_AlterTime is %lld\n", stdInfo->SI_AlterTime);
                fprintf(fp, "SI_DOSAttr is %d\n", stdInfo->SI_DOSAttr);
                fprintf(fp, "SI_MFTChgTime is %lld\n", stdInfo->SI_MFTChgTime);
                fprintf(fp, "\n");
                fprintf(fp, "\n#####END of STANDARD_INFORMATION#####\n\n");
                break;
            }
            case 0x00000020: {//ATTRIBUTE_LIST 20属性
                break;
            }
            case 0x00000030: {//FILE_NAME 可能不止一个
                pFILE_NAME fileNameInfo = (pFILE_NAME) malloc(sizeof(FILE_NAME));
                memcpy(fileNameInfo, tmpAttriDataIndex, sizeof(FILE_NAME));
                fprintf(fp, "\n##### FILE_NAME #####\n");
                //SYSTEMTIME sysTime;
                //FileTimeToSystemTime(&(fileNameInfo->FN_AlterTime), &sysTime);
                //fprintf(fp, "FN_AlterTime-wYear is %hu\n", sysTime.wYear);
                //fprintf(fp, "FN_AlterTime-wMonth is %hu\n", sysTime.wMonth);
                //fprintf(fp, "FN_AlterTime-wDay is %hu\n", sysTime.wDay);
                //fprintf(fp, "FN_AlterTime-wHour is %hu\n", sysTime.wHour);
                //fprintf(fp, "FN_AlterTime-wMinute is %hu\n", sysTime.wMinute);
                //fprintf(fp, "FN_NameSz is %hu\n", 0xFFFF & (fileNameInfo->FN_NameSz));
                //fprintf(fp, "FN_AllocSz is %llu\n", fileNameInfo->FN_AllocSz);
                //fprintf(fp, "FN_ValidSz is %llu\n", fileNameInfo->FN_ValidSz);
                //get file name
                UINT32 fileNameLen = UINT32(0xFFFF & (fileNameInfo->FN_NameSz) + 1) << 1;
                WCHAR *fileName = (WCHAR *) malloc(fileNameLen);
                memset(fileName, 0, fileNameLen);
                memcpy(fileName, tmpAttriDataIndex + sizeof(FILE_NAME), fileNameLen - 2);
                fprintf(fp, "FILENAME is %ls\n", fileName);
                fprintf(fp, "FN_NameSz is %d\n", fileNameInfo->FN_NameSz);
                fprintf(fp, "FN_NamSpace is %d\n", fileNameInfo->FN_NamSpace);
                fprintf(fp, "FILENAME is %ls\n", fileName);
                fprintf(fp, "FN_ParentFR[父目录的MFT号] is %llu\n", (fileNameInfo->FN_ParentFR) & 0XFFFFFFFFFFFF);
                //getchar();
                fprintf(fp, "\n#####END of FILE_NAME#####\n\n");
                break;
            }
            case 0x00000040: {//VOLUME_VERSION  OBJECT_ID
                break;
            }
            case 0x00000050: {//SECURITY_DESCRIPTOR
                break;
            }
            case 0x00000060: {//VOLUME_NAME
                break;
            }
            case 0x00000070: {//VOLUME_INFORMATION
                break;
            }
            case 0x00000080: {//DATA
                break;
            }
            case 0x00000090: {//INDEX_ROOT 索引根

                //pINDEX_ROOT pIndexRoot = (INDEX_ROOT*)malloc(sizeof(INDEX_ROOT));
                //memcpy(pIndexRoot, tmpAttriDataIndex, sizeof(INDEX_ROOT));
                //fprintf(fp, "\n##### INDEX_ROOT #####\n");
                //fprintf(fp, "IR_EntrySz[目录项的大小,一般是一个簇] is %u\n", pIndexRoot->IR_EntrySz);
                //fprintf(fp, "IR_ClusPerRec[目录项占用的簇数,一般是一个] is %u\n", pIndexRoot->IR_ClusPerRec);
                //fprintf(fp, "IH_TalSzOfEntries[索引根和紧随其后的索引项的大小] is %u\n", pIndexRoot->IH.IH_TalSzOfEntries);
                //fprintf(fp, "IH_EntryOff[第一个索引项的偏移] is %u\n", pIndexRoot->IH.IH_EntryOff);
                ////0X90属性的实际大小
                //UINT32 attri90Size = sizeof(INDEX_ROOT) - sizeof(INDEX_HEADER) + pIndexRoot->IH.IH_TalSzOfEntries;
                //fprintf(fp, "\n##### END of INDEX_ROOT #####\n");

                break;
            }
            case 0x000000A0: {//INDEX_ALLOCATION

                //fprintf(fp, "\n##### INDEX_ALLOCATION #####\n");
                ////A0属性体
                //pINDEX_ALLOCATION pIndexAlloc = (pINDEX_ALLOCATION)malloc(sizeof(INDEX_ALLOCATION));
                //memcpy(pIndexAlloc, tmpAttriDataIndex, sizeof(INDEX_ALLOCATION));
                //fprintf(fp, "\n##### END of INDEX_ALLOCATION #####\n");

                break;
            }
            case 0x000000B0: {//BITMAP
                break;
            }
            case 0x000000C0: {//SYMBOL_LINK REPARSE_POINT
                break;
            }
            case 0x000000D0: {//EA_INFORMATION
                break;
            }
            case 0x000000E0: {//EA
                break;
            }
            case 0x000000F0: {//PROPERTY_SET
                break;
            }
            case 0x00000100: {//LOGGED_UNTILITY_STREAM
                break;
            }
            default: {
                finishFlag = TRUE;
                break;
            }
        }
        pIndexOfMFTEntry += pComAttriHeader->ATTR_Size;
        if (finishFlag) { break; }
    }

}


//解析ntfs DBR扇区
//物理磁盘设备号，DBR物理偏移(Byte)，DBR扇区数据
void resolveNTFSDBRSector(DWORD phyDriverNumber, UINT64 startSecOffset, pNTFSDBR DBRBuf) {
    fprintf(fp, "bytePerSector is %hu\n", DBRBuf->bytePerSector);
    fprintf(fp, "secPerCluster is %hu\n", DBRBuf->secPerCluster);
    fprintf(fp, "totalSectors is %llu\n", DBRBuf->totalSectors);
    fprintf(fp, "MFT offset(logical cluster number) is %llu\n", DBRBuf->MFT);
    printf("MFT offset(logical cluster number) is %llu\n", DBRBuf->MFT);
    //ntfs mft offset(byte)
    UINT64 MFToffset = UINT64((DBRBuf->MFT) * (DBRBuf->secPerCluster) * (DBRBuf->bytePerSector)) + startSecOffset;
    fprintf(fp, "MFTMirror offset(logical cluster number) is %llu\n", DBRBuf->MFTMirror);
    fprintf(fp, "\n");
    //直接读取MFT第五项 根目录项
    UINT64 MFTEntryOffset = UINT64(5 * 1024 + MFToffset);
    fprintf(fp, "MFTEntry[5] Offset is %llu\n", MFTEntryOffset);
    PVOID tmpMFTEntryBuf = malloc((UINT) MFTEntrySize);
    memset(tmpMFTEntryBuf, 0, (UINT) MFTEntrySize);
    ReadDisk(phyDriverNumber, MFTEntryOffset, MFTEntrySize, tmpMFTEntryBuf);
    //路径
    WCHAR filePath[2048] = {0};
    //这里开始深搜目录树
    parseMFTEntry(tmpMFTEntryBuf, MFTEntrySize, phyDriverNumber, startSecOffset,
                  DBRBuf->secPerCluster, UINT64((DBRBuf->MFT) * (DBRBuf->secPerCluster) * (DBRBuf->bytePerSector)),
                  filePath);
//	return;
    //这里是顺序读取MFT 能读到被删的文件信息
    //读取MFT表项
    UINT64 MFTEntryCnt = 0;
    UINT32 mftEntryFlag;
    printf("Analyzing MFT......\n");
    printf("MFTEntryCnt");
    while (TRUE) {
        //while (MFTEntryCnt<128) {
        fprintf(fp, "\n\n########### MFT ENTRY ##########\n");
        fprintf(fp, "MFTEntryCnt is %llu\n", MFTEntryCnt);
        printf("%llu\t", MFTEntryCnt);
        //PVOID tmpMFTEntryBuf = malloc((UINT)MFTEntrySize);
        //get MFTEntry
        UINT64 MFTEntryOffset = UINT64((MFTEntryCnt++) * 1024 + MFToffset);
        fprintf(fp, "MFTEntryOffset is %llu\n", MFTEntryOffset);
        PVOID tmpMFTEntryBuf = malloc((UINT) MFTEntrySize);
        memset(tmpMFTEntryBuf, 0, (UINT) MFTEntrySize);
        ReadDisk(phyDriverNumber, MFTEntryOffset, MFTEntrySize, tmpMFTEntryBuf);
        //解析MFT表项
        mftEntryFlag = 0X00;
        memcpy(&mftEntryFlag, tmpMFTEntryBuf, (UINT) 4);
        if (mftEntryFlag != MFTEntryFlag) {
            fprintf(fp, "\nMiss MFTEntryFlag, break\n");
            break;
        }
        //printBuffer(tmpMFTEntryBuf, MFTEntrySize);
        resolveMFTEntry(tmpMFTEntryBuf, phyDriverNumber, startSecOffset, DBRBuf->secPerCluster,
                        UINT64((DBRBuf->MFT) * (DBRBuf->secPerCluster) * (DBRBuf->bytePerSector)));
        fprintf(fp, "\n########### END MFT ENTRY ##########\n");
    }
    printf("\n");
}

void run() {
    fp = fopen("output.txt", "w");
    vector<PHY_INFO> driverInfos = getPhyDriverNumber();

    pMBRSector MBRs[64];

    short int MBRcnt = 0;
    for (int i = 0; i < driverInfos.size(); i++) {
        DWORD phyVolCnt = 0;
        //phyVolCnt = 0;
        fprintf(fp, "### NOW IN PHYDRIVER - %d ###\n", driverInfos[i].number);
        printf("### NOW IN PHYDRIVER - %d ###\n", driverInfos[i].number);
        //MBRs[MBRcnt] = (pMBRSector)malloc(sizeof(MBRSector));
        //read MBR sector
        PVOID tmpMBRBuf = malloc(UINT32(MBRSectorSize));
        ReadDisk(driverInfos[i].number, 0, MBRSectorSize, tmpMBRBuf);
        MBRs[MBRcnt] = (pMBRSector) tmpMBRBuf;
        //for test
        fprintf(fp, "MBR in physicalDriveNumber%lu:\n", driverInfos[i].number);
        printf("MBR in physicalDriveNumber%lu:\n", driverInfos[i].number);
        printBuffer(tmpMBRBuf, MBRSectorSize);
        printBuffer2(tmpMBRBuf, MBRSectorSize);
        fprintf(fp, "\n");
        int PTEntryCnt = 0;
        while (PTEntryCnt < 4) {
            fprintf(fp, "PartitionTableEntry%d:\n", PTEntryCnt);
            fprintf(fp, "bootSignature(引导标志) is %+02X\n", MBRs[MBRcnt]->ptEntrys[PTEntryCnt].bootSignature);
            fprintf(fp, "systemSignature(分区类型标志) is %+02X\n", MBRs[MBRcnt]->ptEntrys[PTEntryCnt].systemSignature);
            fprintf(fp, "startSectorNo is %+08X\n", MBRs[MBRcnt]->ptEntrys[PTEntryCnt].startSectorNo);
            //printf("startSectorNo is %+08X\n", MBRs[MBRcnt - 1]->ptEntrys[PTEntryCnt].startSectorNo);
            fprintf(fp, "totalSectorsNum is %+08X\n", MBRs[MBRcnt]->ptEntrys[PTEntryCnt].totalSectorsNum);
            fprintf(fp, "\n");
            //PTEntryCnt不足四个
            if (MBRs[MBRcnt]->ptEntrys[PTEntryCnt].startSectorNo == 0x00) {
                break;
            }

            //先跳过活动分区
            if (MBRs[MBRcnt]->ptEntrys[PTEntryCnt].bootSignature == 0X80) {
                printf("\n\n\nVolume letter is %c\n\n\n", driverInfos[i].vols[phyVolCnt]);
                printf("\n\n活动分区, leap it\n");
                fprintf(fp, "\n\n活动分区, leap it\n");
                PTEntryCnt++;
                phyVolCnt++;
                getchar();
                continue;
            }
            //read DBR or EBR sector
            PVOID readBuf = malloc(UINT32(SectorSize));
            UINT64 startSecOffset = UINT64(MBRs[MBRcnt]->ptEntrys[PTEntryCnt].startSectorNo);
            startSecOffset *= UINT64(SectorSize);

            fprintf(fp, "startSecOffset is %lld\n", startSecOffset);
            ReadDisk(driverInfos[i].number, startSecOffset, SectorSize, readBuf);

            //判断分区类型
            switch (MBRs[MBRcnt]->ptEntrys[PTEntryCnt].systemSignature) {
                case BYTE(0x0C): {//FAT32
                    printf("\n\n\nVolume letter is %c\n\n\n", driverInfos[i].vols[phyVolCnt++]);
                    getchar();
                    fprintf(fp, "FAT32 DBR sector is\n");
                    //printf("FAT32 DBR sector, continue\n");
                    printBuffer(readBuf, SectorSize);
                    fprintf(fp, "\n");
                    pFAT32_DBR DBRBuf = (pFAT32_DBR) readBuf;
                    //sizeof(FAT32_DBR);
                    fprintf(fp, "Sectors_per_Cluster is %hu\n", DBRBuf->BPB.Sectors_per_Cluster);
                    fprintf(fp, "FATs is %hu\n", DBRBuf->BPB.FATs);
                    fprintf(fp, "FATs is %u\n", DBRBuf->BPB.Large_Sector);
                    fprintf(fp, "System_ID is %llu\n", DBRBuf->Extend_BPB.System_ID);
                    fprintf(fp, "\n");

                    printf("resolve fat32 volume\n");
                    //每扇区字节数
                    UINT16 bytesPerSector = DBRBuf->BPB.Bytes_per_Sector;
                    //每簇扇区数
                    UINT8 sectorsPerCluster = DBRBuf->BPB.Sectors_per_Cluster;
                    //保留扇区数
                    UINT16 reservedSectorNum = DBRBuf->BPB.Reserved_Sector;
                    //fat表占用的扇区数
                    UINT32 fat32Sectors = DBRBuf->BPB.Fat32_Sector.Sectors_per_FAT_FAT32;
                    //根目录簇号
                    UINT32 rootCluster = DBRBuf->BPB.Fat32_Sector.Root_Cluster_Number;
                    //定位根目录  保留扇区数+每个FAT表占用扇区数*2+(根目录簇号-2)*每簇扇区数
                    UINT64 rootSectorOffset =
                            reservedSectorNum + fat32Sectors * 2 + (rootCluster - 2) * sectorsPerCluster;

                    printf("rootSectorOffset is %llu\n", rootSectorOffset);
                    getchar();
                    UINT64 rootSectorByteOffset = rootSectorOffset * bytesPerSector + startSecOffset;

                    PVOID tmpBuff = malloc(SectorSize);
                    ReadDisk(driverInfos[i].number, rootSectorByteOffset, SectorSize, tmpBuff);

                    printBuffer2(tmpBuff, SectorSize);
                    getchar();

                    break;
                }
                case BYTE(0x07): {//ntfs
                    printf("\n\n\nVolume letter is %c\n\n\n", driverInfos[i].vols[phyVolCnt++]);
                    getchar();
                    fprintf(fp, "ntfs DBR sector is\n");
                    printf("find ntfs DBR sector\n");
                    printBuffer(readBuf, SectorSize);
                    fprintf(fp, "\n");
                    pNTFSDBR DBRBuf = (pNTFSDBR) readBuf;
                    //解析ntfs dbr
                    resolveNTFSDBRSector(driverInfos[i].number, startSecOffset, DBRBuf);

                    break;
                }
                case BYTE(0x0F): {//Extended
                    fprintf(fp, "EBR sector is\n");
                    printBuffer(readBuf, SectorSize);
                    fprintf(fp, "\n");
                    pMBRSector EBRBuf = (pMBRSector) readBuf;
                    bool isEbrFinish = FALSE;
                    while (TRUE) {
                        if (!(EBRBuf->ptEntrys[1].totalSectorsNum)) { isEbrFinish = TRUE; }
                        printf("find EBR sector, continue\n");
                        fprintf(fp, "this volume : logical startSectorNo is %+08X\n",
                                EBRBuf->ptEntrys[0].startSectorNo);
                        fprintf(fp, "this volume : totalSectorsNum is %+08X\n", EBRBuf->ptEntrys[0].totalSectorsNum);
                        fprintf(fp, "next volume : logical startSectorNo is %+08X\n",
                                EBRBuf->ptEntrys[1].startSectorNo);
                        //本分区DBR的物理偏移
                        UINT64 thisVolPhyStartSecOffset = UINT64(MBRs[MBRcnt]->ptEntrys[PTEntryCnt].startSectorNo)
                                                          + UINT64(EBRBuf->ptEntrys[0].startSectorNo);
                        thisVolPhyStartSecOffset *= UINT64(SectorSize);
                        //下一个EBR的物理偏移
                        UINT64 nextVolPhyStartSecOffset = UINT64(MBRs[MBRcnt]->ptEntrys[PTEntryCnt].startSectorNo)
                                                          + UINT64(EBRBuf->ptEntrys[1].startSectorNo);
                        nextVolPhyStartSecOffset *= UINT64(SectorSize);
                        printf("nextVolPhyStartSecOffset is %llu\n", nextVolPhyStartSecOffset);
                        getchar();
                        //读取本分区的DBR
                        PVOID tempDBRBuf = malloc(int(DBRSectorSize));
                        ReadDisk(driverInfos[i].number, thisVolPhyStartSecOffset, DBRSectorSize, tempDBRBuf);
                        fprintf(fp, "\nDBR after EBR:\n");
                        printBuffer(tempDBRBuf, DBRSectorSize);
                        fprintf(fp, "\n");

                        switch (EBRBuf->ptEntrys[0].systemSignature) {
                            case BYTE(0x07): {//ntfs
                                printf("\n\n\nVolume letter is %c\n\n\n", driverInfos[i].vols[phyVolCnt++]);
                                getchar();
                                printf("find ntfs DBR sector\n");
                                pNTFSDBR DBRBuf = (pNTFSDBR) tempDBRBuf;
                                //解析ntfs dbr
                                resolveNTFSDBRSector(driverInfos[i].number, thisVolPhyStartSecOffset, DBRBuf);
                                break;
                            }
                            case BYTE(0x0C): {//fat32
                                printf("\n\n\nVolume letter is %c\n\n\n", driverInfos[i].vols[phyVolCnt++]);
                                getchar();
                                printf("FAT32 DBR sector, continue\n");
                                break;
                            }
                            default:
                                break;
                        }
                        if (isEbrFinish) { break; }
                        //读取下一个EBR
                        PVOID tempEBRBuf = malloc(int(EBRSectorSize));
                        ReadDisk(driverInfos[i].number, nextVolPhyStartSecOffset, EBRSectorSize, tempEBRBuf);
                        EBRBuf = pMBRSector(tempEBRBuf);
                        fprintf(fp, "\nnext EBR sector is\n");
                        printBuffer(tempEBRBuf, EBRSectorSize);
                        fprintf(fp, "\n");
                    }
                    break;
                }
                default:
                    break;
            }
            PTEntryCnt++;
            fprintf(fp, "\n");
        }
        MBRcnt++;
        fprintf(fp, "\n");
    }
    fclose(fp);
}

//判断卷是否属于USB
bool isUsbDev(TCHAR volumePath[]) {
    HANDLE deviceHandle = CreateFile(volumePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    STORAGE_PROPERTY_QUERY query;
    memset(&query, 0, sizeof(query));

    DWORD bytes;
    STORAGE_DEVICE_DESCRIPTOR devd;

    //STORAGE_BUS_TYPE用于记录结构，类型要初始化
    STORAGE_BUS_TYPE busType = BusTypeUnknown;

    if (DeviceIoControl(deviceHandle, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), &devd, sizeof(devd), &bytes,
                        NULL)) {
        busType = devd.BusType;

    }
    CloseHandle(deviceHandle);
    return busType == BusTypeUsb;
}

//发现USB设备
bool findUsbDev() {
    bool ret = false;
    DWORD dwSize = GetLogicalDriveStrings(0, NULL);
    char *drivers = (char *) malloc(dwSize * 2);
    DWORD dwRet = GetLogicalDriveStrings(dwSize, (LPWSTR) drivers);
    wchar_t *lp = (wchar_t *) drivers;//所有逻辑驱动器的根驱动器路径 用0隔开
    DWORD tmpNum = 0;
    while (*lp) {
        CHAR path[MAX_PATH];
        sprintf(path, "\\\\.\\%c:", lp[0]);
        if (isUsbDev(charToWCHAR(path))) {
            //printf("find usb\n");
            ret = true;
            break;
        }
        lp += (wcslen(lp) + 1);//下一个根驱动器路径
    }
    return ret;
}

void test() {

}

int   _tmain(int argc, TCHAR *argv[], TCHAR *env[]) {

    setlocale(LC_ALL, "chs");

    //test();
    run();

    system("pause");
    return 0;
}