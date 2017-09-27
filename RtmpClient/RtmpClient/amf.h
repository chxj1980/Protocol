#ifndef __AMF_H__
#define __AMF_H__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "bytes.h"
#include "stdint.h"

#ifndef TRUE
#define TRUE	1
#define FALSE	0
#endif

/************************************************************************************************************
*	AMF�ֳ�����;
*	1. AMF0������������ת������
*	2. AMF3����AMF0����չ
************************************************************************************************************/   

// AMF0��������;
typedef enum
{
	AMF_NUMBER = 0,			// ����(double);
	AMF_BOOLEAN,			// ����;
	AMF_STRING,				// �ַ���;
	AMF_OBJECT,				// ����;
	AMF_MOVIECLIP,			// ����,δʹ��;
	AMF_NULL,				// null;
	AMF_UNDEFINED,			// δ����;
	AMF_REFERENCE,			// ����;
	AMF_ECMA_ARRAY,			// ����;
	AMF_OBJECT_END,			// �������(0x09);
	AMF_STRICT_ARRAY,		// �ϸ������;
	AMF_DATE,				// ����;
	AMF_LONG_STRING,		// ���ַ���;
	AMF_UNSUPPORTED,		// δ֧��;
	AMF_RECORDSET,			// ����,δʹ��;
	AMF_XML_DOC,			// xml�ĵ�;
	AMF_TYPED_OBJECT,		// �����͵Ķ���;
	AMF_AVMPLUS,			// ��Ҫ��չ��AMF3;
	AMF_INVALID = 0xff		// ��Ч��;
}AMFDataType;

// AMF3��������;
typedef enum
{
	AMF3_UNDEFINED = 0,		// δ����;
	AMF3_NULL,				// null;
	AMF3_FALSE,				// false;
	AMF3_TRUE,				// true;
	AMF3_INTEGER,			// ����int;
	AMF3_DOUBLE,			// double;
	AMF3_STRING,			// �ַ���;
	AMF3_XML_DOC,			// xml�ĵ�;
	AMF3_DATE,				// ����;
	AMF3_ARRAY,				// ����;
	AMF3_OBJECT,			// ����;
	AMF3_XML,				// xml;
	AMF3_BYTE_ARRAY			// �ֽ�����;
} AMF3DataType;

// AMF�Զ�����ַ���;
typedef struct AVal
{
	char *av_val;
	int av_len;
} AVal;

// AVal�Ŀ��ٳ�ʼ��;
#define AVC(str)		{str, sizeof(str)-1}
// �Ƚ�AVal�ַ���;
#define AVMATCH(a1,a2)	((a1)->av_len == (a2)->av_len && !memcmp((a1)->av_val,(a2)->av_val,(a1)->av_len))

static const AVal sg_emptyVal = AVC(NULL);

class AMFObjectProperty;
// AMF����, ������һϵ�е����Թ��ɵ�;
class AMFObject
{
public:
	// ����;
	static char* EncodeInt16(OUT char* strOutPut, OUT char* strOutEnd, IN const short& iVal);
	static char* EncodeInt24(OUT char* strOutPut, OUT char* strOutEnd, IN const int& iVal);
	static char* EncodeInt32(OUT char* strOutPut, OUT char* strOutEnd, IN const int& iVal);
	static char* EncodeString(OUT char* strOutPut, OUT char* strOutEnd, IN const AVal& strVal);
	static char* EncodeNumber(OUT char* strOutPut, OUT char* strOutEnd, IN const double& dVal);
	static char* EncodeBoolean(OUT char* strOutPut, OUT char* strOutEnd, IN const int& bVal);
	// ����;
	static unsigned short DecodeInt16(IN const char* strInput);
	static unsigned int DecodeInt24(IN const char* strInput);
	static unsigned int DecodeInt32(IN const char* strInput);
	static void DecodeString(OUT AVal&  strVal, IN const char* strInput);
	static void DecodeLongString(OUT AVal&  strVal, IN const char* strInput);
	static double DecodeNumber(IN const char* strInput);
	static bool DecodeBoolean(IN const char* strInput);

	// AMF3��ȡ����
	static int AMF3ReadInteger(OUT int32_t& iVal, IN const char* strInput);
	// AMF3��ȡ�ַ���
	static int AMF3ReadString(OUT AVal& strVal, IN const char* strInput);

public:
	AMFObject();
	~AMFObject();

	// AMFProp_Encode��ݱ���(���name+value);
	static char* EncodeNamedString( OUT char* strOutPut, OUT char* strOutEnd, IN const AVal& strName, IN const AVal&  strVal);
	static char* EncodeNamedNumber( OUT char* strOutPut, OUT char* strOutEnd, IN const AVal& strName, IN const double& dVal);
	static char* EncodeNamedBoolean(OUT char* strOutPut, OUT char* strOutEnd, IN const AVal& strName, IN const int& bVal);

	// ����
	char* Encode(OUT char* strOutPut, OUT char* strOutEnd);
	char* EncodeEcmaArray(OUT char* strOutPut, OUT char* strOutEnd);
	char* EncodeArray(OUT char* strOutPut, OUT char* strOutEnd);
	// ����
	int Decode(IN const char* strInput, IN int iSize, IN bool bDecodeName);
	int DecodeArray(IN const char* strInput, IN int iSize, IN int iArrayLen, IN bool bDecodeName);
	int Decode_AMF3(IN const char* strInput, IN int iSize, IN bool bDecodeName);
	
	// ������ʾ
	void Dump();
	// ����
	void Reset();

	void AddProp(IN const AVal& strName, IN const double& dVal);
	void AddProp(IN const AVal& strName, IN const AVal& strVal);
	void AddProp(IN const AVal& strName, IN const AMFObject& objVal);

	void AddProp(IN const AMFObjectProperty* pProp);
	int GetPropCount();
	AMFObjectProperty* GetObjectProp(IN const AVal& strName, IN const int iIndex);

public:
	int o_num;					// ������Ŀ;
	AMFObjectProperty *o_props;	// ��������;
};

union AMFVal
{
	AMFVal():p_number(0) {}
	~AMFVal() {}

	double		p_number;
	AVal		p_aval;
	AMFObject	p_object;
};

// AMF���������;
class AMFObjectProperty
{
public:
	AMFObjectProperty();
	~AMFObjectProperty();

public:
	// ����
	void SetName(IN const AVal& strName);
	void GetName(OUT AVal& strName);
	// ����
	void SetType(IN const AMFDataType& iType);
	AMFDataType GetType();
	
/**********************************����****************************************/
	// ����
	void SetNumber(IN const double& dVal);
	double GetNumber();
	void SetBoolean(IN const bool& bVal);
	bool GetBoolean();

	void SetString(IN const AVal& strVal);
	void GetString(IN AVal& strVal);

	void SetObject(IN const AMFObject& objVal);
	void GetObject(IN AMFObject& objVal);
/**********************************����****************************************/

	// �Ƿ���Ч;
	int IsValid();

	char* Encode(OUT char* strOutPut, OUT char* strOutEnd);
	int Decode(IN const char* strInput, IN int& iSize, IN const bool& bDecodeName);
	int Decode_AMF3(IN const char* strInput, IN int& iSize, IN const bool& bDecodeName);
	
	// ������ʾ;
	void Dump();
	// ����;
	void Reset();

public:
	AVal		p_name;			// ��������;
	AMFDataType p_type;			// ��������;
	AMFVal		p_vu;			// ������ֵ;
	int16_t		p_UTCoffset;	// UTCƫ��;
};

// AMF3����
class AMF3ClassDef
{
public:
	void AMF3CD_AddProp(AVal*  pProp);
	AVal* AMF3CD_GetProp(int iIndex);

public:
	AVal	cd_name;
	char	cd_externalizable;
	char	cd_dynamic;
	int		cd_num;
	AVal*	cd_props;
};

#endif				/* __AMF_H__ */