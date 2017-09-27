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
*	AMF分成两种;
*	1. AMF0，基本的数据转换规则
*	2. AMF3，是AMF0的扩展
************************************************************************************************************/   

// AMF0数据类型;
typedef enum
{
	AMF_NUMBER = 0,			// 数字(double);
	AMF_BOOLEAN,			// 布尔;
	AMF_STRING,				// 字符串;
	AMF_OBJECT,				// 对象;
	AMF_MOVIECLIP,			// 保留,未使用;
	AMF_NULL,				// null;
	AMF_UNDEFINED,			// 未定义;
	AMF_REFERENCE,			// 引用;
	AMF_ECMA_ARRAY,			// 数组;
	AMF_OBJECT_END,			// 对象结束(0x09);
	AMF_STRICT_ARRAY,		// 严格的数组;
	AMF_DATE,				// 日期;
	AMF_LONG_STRING,		// 长字符串;
	AMF_UNSUPPORTED,		// 未支持;
	AMF_RECORDSET,			// 保留,未使用;
	AMF_XML_DOC,			// xml文档;
	AMF_TYPED_OBJECT,		// 有类型的对象;
	AMF_AVMPLUS,			// 需要扩展到AMF3;
	AMF_INVALID = 0xff		// 无效的;
}AMFDataType;

// AMF3数据类型;
typedef enum
{
	AMF3_UNDEFINED = 0,		// 未定义;
	AMF3_NULL,				// null;
	AMF3_FALSE,				// false;
	AMF3_TRUE,				// true;
	AMF3_INTEGER,			// 数字int;
	AMF3_DOUBLE,			// double;
	AMF3_STRING,			// 字符串;
	AMF3_XML_DOC,			// xml文档;
	AMF3_DATE,				// 日期;
	AMF3_ARRAY,				// 数组;
	AMF3_OBJECT,			// 对象;
	AMF3_XML,				// xml;
	AMF3_BYTE_ARRAY			// 字节数组;
} AMF3DataType;

// AMF自定义的字符串;
typedef struct AVal
{
	char *av_val;
	int av_len;
} AVal;

// AVal的快速初始化;
#define AVC(str)		{str, sizeof(str)-1}
// 比较AVal字符串;
#define AVMATCH(a1,a2)	((a1)->av_len == (a2)->av_len && !memcmp((a1)->av_val,(a2)->av_val,(a1)->av_len))

static const AVal sg_emptyVal = AVC(NULL);

class AMFObjectProperty;
// AMF对象, 就是由一系列的属性构成的;
class AMFObject
{
public:
	// 编码;
	static char* EncodeInt16(OUT char* strOutPut, OUT char* strOutEnd, IN const short& iVal);
	static char* EncodeInt24(OUT char* strOutPut, OUT char* strOutEnd, IN const int& iVal);
	static char* EncodeInt32(OUT char* strOutPut, OUT char* strOutEnd, IN const int& iVal);
	static char* EncodeString(OUT char* strOutPut, OUT char* strOutEnd, IN const AVal& strVal);
	static char* EncodeNumber(OUT char* strOutPut, OUT char* strOutEnd, IN const double& dVal);
	static char* EncodeBoolean(OUT char* strOutPut, OUT char* strOutEnd, IN const int& bVal);
	// 解码;
	static unsigned short DecodeInt16(IN const char* strInput);
	static unsigned int DecodeInt24(IN const char* strInput);
	static unsigned int DecodeInt32(IN const char* strInput);
	static void DecodeString(OUT AVal&  strVal, IN const char* strInput);
	static void DecodeLongString(OUT AVal&  strVal, IN const char* strInput);
	static double DecodeNumber(IN const char* strInput);
	static bool DecodeBoolean(IN const char* strInput);

	// AMF3读取数字
	static int AMF3ReadInteger(OUT int32_t& iVal, IN const char* strInput);
	// AMF3读取字符串
	static int AMF3ReadString(OUT AVal& strVal, IN const char* strInput);

public:
	AMFObject();
	~AMFObject();

	// AMFProp_Encode快捷编码(添加name+value);
	static char* EncodeNamedString( OUT char* strOutPut, OUT char* strOutEnd, IN const AVal& strName, IN const AVal&  strVal);
	static char* EncodeNamedNumber( OUT char* strOutPut, OUT char* strOutEnd, IN const AVal& strName, IN const double& dVal);
	static char* EncodeNamedBoolean(OUT char* strOutPut, OUT char* strOutEnd, IN const AVal& strName, IN const int& bVal);

	// 编码
	char* Encode(OUT char* strOutPut, OUT char* strOutEnd);
	char* EncodeEcmaArray(OUT char* strOutPut, OUT char* strOutEnd);
	char* EncodeArray(OUT char* strOutPut, OUT char* strOutEnd);
	// 解码
	int Decode(IN const char* strInput, IN int iSize, IN bool bDecodeName);
	int DecodeArray(IN const char* strInput, IN int iSize, IN int iArrayLen, IN bool bDecodeName);
	int Decode_AMF3(IN const char* strInput, IN int iSize, IN bool bDecodeName);
	
	// 调试显示
	void Dump();
	// 重置
	void Reset();

	void AddProp(IN const AVal& strName, IN const double& dVal);
	void AddProp(IN const AVal& strName, IN const AVal& strVal);
	void AddProp(IN const AVal& strName, IN const AMFObject& objVal);

	void AddProp(IN const AMFObjectProperty* pProp);
	int GetPropCount();
	AMFObjectProperty* GetObjectProp(IN const AVal& strName, IN const int iIndex);

public:
	int o_num;					// 属性数目;
	AMFObjectProperty *o_props;	// 属性数组;
};

union AMFVal
{
	AMFVal():p_number(0) {}
	~AMFVal() {}

	double		p_number;
	AVal		p_aval;
	AMFObject	p_object;
};

// AMF对象的属性;
class AMFObjectProperty
{
public:
	AMFObjectProperty();
	~AMFObjectProperty();

public:
	// 名称
	void SetName(IN const AVal& strName);
	void GetName(OUT AVal& strName);
	// 类型
	void SetType(IN const AMFDataType& iType);
	AMFDataType GetType();
	
/**********************************变量****************************************/
	// 数字
	void SetNumber(IN const double& dVal);
	double GetNumber();
	void SetBoolean(IN const bool& bVal);
	bool GetBoolean();

	void SetString(IN const AVal& strVal);
	void GetString(IN AVal& strVal);

	void SetObject(IN const AMFObject& objVal);
	void GetObject(IN AMFObject& objVal);
/**********************************变量****************************************/

	// 是否有效;
	int IsValid();

	char* Encode(OUT char* strOutPut, OUT char* strOutEnd);
	int Decode(IN const char* strInput, IN int& iSize, IN const bool& bDecodeName);
	int Decode_AMF3(IN const char* strInput, IN int& iSize, IN const bool& bDecodeName);
	
	// 调试显示;
	void Dump();
	// 重置;
	void Reset();

public:
	AVal		p_name;			// 属性名称;
	AMFDataType p_type;			// 属性类型;
	AMFVal		p_vu;			// 属性数值;
	int16_t		p_UTCoffset;	// UTC偏移;
};

// AMF3类型
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