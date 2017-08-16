#include "amf.h"

#define AMF3_INTEGER_MAX	268435455
#define AMF3_INTEGER_MIN	-268435456

static const AMFObjectProperty AMFProp_Invalid = { { 0, 0 }, AMF_INVALID };
static const AVal AV_empty = { 0, 0 };

/************************************************************************************************************
*	AMF3��ȡ����;
*
************************************************************************************************************/
int AMFObject::AMF3ReadInteger(OUT int32_t& iVal, IN const char* strInput)
{
	int i = 0;
	int32_t val = 0;
	while (i <= 2)
	{
		/* handle first 3 bytes */
		if (strInput[i] & 0x80)
		{
			/* byte used */
			val <<= 7;					/* shift up */
			val |= (strInput[i] & 0x7f);	/* add bits */
			i++;
		}
		else
		{
			break;
		}
	}

	if (i > 2)
	{
		/* use 4th byte, all 8bits */
		val <<= 8;
		val |= strInput[3];

		/* range check */
		if (val > AMF3_INTEGER_MAX)
		{
			val -= (1 << 29);
		}
	}
	else
	{
		/* use 7bits of last unparsed byte (0xxxxxxx) */
		val <<= 7;
		val |= strInput[i];
	}

	iVal = val;

	return i > 2 ? 4 : i + 1;
}

/************************************************************************************************************
*	AMF3��ȡ�ַ���;
*
************************************************************************************************************/
int AMFObject::AMF3ReadString(OUT AVal& strVal, IN const char* strInput)
{
	int32_t ref = 0;
	int len;
	assert(&strVal != 0);

	len = AMF3ReadInteger(ref, strInput);
	strInput += len;

	if ((ref & 0x1) == 0)
	{
		/* reference: 0xxx */
		uint32_t refIndex = (ref >> 1);
		//RTMP_Log(//RTMP_LOGDEBUG, "%s, string reference, index: %d, not supported, ignoring!", __FUNCTION__, refIndex);
		return len;
	}
	else
	{
		uint32_t nSize = (ref >> 1);

		strVal.av_val = (char*)strInput;
		strVal.av_len = nSize;

		return len + nSize;
	}
	return len;
}

/************************************************************************************************************
*	����strName+strValue;
*
*	name : ����+����;
:	value: string����;
************************************************************************************************************/
char* AMFObject::EncodeNamedString(OUT char* strOutPut, OUT char* strOutEnd, IN const AVal& strName, IN const AVal&  strVal)
{
	if (strOutPut + 2 + strName.av_len > strOutEnd)
	{
		return NULL;
	}
	strOutPut = EncodeInt16(strOutPut, strOutEnd, strName.av_len);

	memcpy(strOutPut, strName.av_val, strName.av_len);
	strOutPut += strName.av_len;

	return EncodeString(strOutPut, strOutEnd, strVal);
}

/************************************************************************************************************
*	����strName+dVal;
*
*	name : ����+����;
:	value: double����;
************************************************************************************************************/
char* AMFObject::EncodeNamedNumber(OUT char* strOutPut, OUT char* strOutEnd, IN const AVal& strName, IN const double& dVal)
{
	if (strOutPut + 2 + strName.av_len > strOutEnd)
	{
		return NULL;
	}
	strOutPut = EncodeInt16(strOutPut, strOutEnd, strName.av_len);

	memcpy(strOutPut, strName.av_val, strName.av_len);
	strOutPut += strName.av_len;

	return EncodeNumber(strOutPut, strOutEnd, dVal);
}

/************************************************************************************************************
*	����strName+dVal;
*
*	name : ����+����;
:	value: bool����;
************************************************************************************************************/
char* AMFObject::EncodeNamedBoolean(OUT char* strOutPut, OUT char* strOutEnd, IN const AVal& strName, IN const int& bVal)
{
	if (strOutPut + 2 + strName.av_len > strOutEnd)
	{
		return NULL;
	}
	strOutPut = EncodeInt16(strOutPut, strOutEnd, strName.av_len);

	memcpy(strOutPut, strName.av_val, strName.av_len);
	strOutPut += strName.av_len;

	return EncodeBoolean(strOutPut, strOutEnd, bVal);
}

/************************************************************************************************************
*	����: pBuffer;
*
************************************************************************************************************/
char* AMFObject::Encode(OUT char* strOutPut, OUT char* strOutEnd)
{
	int i;
	if (strOutPut + 4 >= strOutEnd)
	{
		return NULL;
	}
	*strOutPut++ = AMF_OBJECT;

	for (i = 0; i < o_num; i++)
	{
		// �����������α���;
		char* res = o_props[i].Encode(strOutPut, strOutEnd);
		if (res == NULL)
		{
			//RTMP_Log(//RTMP_LOGERROR, "Encode - failed to encode property in index %d", i);
			break;
		}
		else
		{
			strOutPut = res;
		}
	}

	if (strOutPut + 3 >= strOutEnd)
	{
		return NULL;			/* no room for the end marker */
	}

	// oject�Ķ�����Ҫ��009��β��ʶ;
	strOutPut = EncodeInt24(strOutPut, strOutEnd, AMF_OBJECT_END);

	return strOutPut;
}

/************************************************************************************************************
*	����: pBuffer;
*
************************************************************************************************************/
char* AMFObject::EncodeEcmaArray(OUT char* strOutPut, OUT char* strOutEnd)
{
	if (strOutPut + 4 >= strOutEnd)
	{
		return NULL;
	}
	*strOutPut++ = AMF_ECMA_ARRAY;

	// ������Ҫ�Ѹ��������ȥ;
	strOutPut = EncodeInt32(strOutPut, strOutEnd, o_num);
	for (int i = 0; i < o_num; i++)
	{
		char* res = o_props[i].Encode(strOutPut, strOutEnd);
		if (res == NULL)
		{
			//RTMP_Log(//RTMP_LOGERROR, "Encode - failed to encode property in index %d", i);
			break;
		}
		else
		{
			strOutPut = res;
		}
	}

	if (strOutPut + 3 >= strOutEnd)
	{
		return NULL;			/* no room for the end marker */
	}

	// oject�Ķ�����Ҫ��009��β��ʶ;
	strOutPut = EncodeInt24(strOutPut, strOutEnd, AMF_OBJECT_END);

	return strOutPut;
}

/************************************************************************************************************
*	����: pBuffer;
*
************************************************************************************************************/
char* AMFObject::EncodeArray(OUT char* strOutPut, OUT char* strOutEnd)
{
	int i;
	if (strOutPut + 4 >= strOutEnd)
	{
		return NULL;
	}
	*strOutPut++ = AMF_STRICT_ARRAY;

	// ������Ҫ�Ѹ��������ȥ;
	strOutPut = EncodeInt32(strOutPut, strOutEnd, o_num);
	for (i = 0; i < o_num; i++)
	{
		char* res = o_props[i].Encode(strOutPut, strOutEnd);
		if (res == NULL)
		{
			//RTMP_Log(//RTMP_LOGERROR, "Encode - failed to encode property in index %d", i);
			break;
		}
		else
		{
			strOutPut = res;
		}
	}

	// �˴�oject�Ķ�����Ҫ��009��β��ʶ;
	//if (pBuffer + 3 >= pBufEnd)
	//  return NULL;			/* no room for the end marker */

	//pBuffer = EncodeInt24(pBuffer, pBufEnd, AMF_OBJECT_END);

	return strOutPut;
}

/************************************************************************************************************
*	����: pBuffer->obj;
*
************************************************************************************************************/
int AMFObject::Decode(IN const char* strInput, IN int iSize, IN bool bDecodeName)
{
	int nOriginalSize = iSize;

	int bError = FALSE;
	/* if there is an error while decoding - try to at least find the end mark AMF_OBJECT_END */
	// ����������,�᳢�Բ���009������ʶ��;

	o_num = 0;
	o_props = NULL;
	while (iSize > 0)
	{
		AMFObjectProperty prop;
		int nRes;

		if (iSize >= 3 && DecodeInt24(strInput) == AMF_OBJECT_END)
		{
			iSize -= 3;
			bError = FALSE;
			break;
		}

		if (bError)
		{
			//RTMP_Log(//RTMP_LOGERROR, "DECODING ERROR, IGNORING BYTES UNTIL NEXT KNOWN PATTERN!");
			iSize--;
			strInput++;
			continue;
		}

		nRes = prop.Decode(strInput, iSize, bDecodeName);
		if (nRes == -1)
		{
			bError = TRUE;
		}
		else
		{
			// �������������׷�ӵ�obj��;
			iSize -= nRes;
			strInput += nRes;
			AddProp(&prop);
		}
	}

	if (bError)
	{
		return -1;
	}

	return nOriginalSize - iSize;
}

/************************************************************************************************************
*	����: pBuffer->obj;
*
************************************************************************************************************/
int AMFObject::DecodeArray(IN const char* strInput, IN int iSize, IN int iArrayLen, IN bool bDecodeName)
{
	int nOriginalSize = iSize;
	int bError = FALSE;

	o_num = 0;
	o_props = NULL;
	while (iArrayLen > 0)
	{
		AMFObjectProperty prop;
		int nRes;
		iArrayLen--;

		nRes = prop.Decode(strInput, iSize, bDecodeName);
		if (nRes == -1)
		{
			bError = TRUE;
		}
		else
		{
			// �������������׷�ӵ�obj��;
			iSize -= nRes;
			strInput += nRes;
			AddProp(&prop);
		}
	}
	if (bError)
	{
		return -1;
	}

	return nOriginalSize - iSize;
}

int AMFObject::Decode_AMF3(IN const char* strInput, IN int iSize, IN bool bDecodeName)
{
	int nOriginalSize = iSize;
	int32_t ref;
	int len;

	o_num = 0;
	o_props = NULL;
	if (bDecodeName)
	{
		if (*strInput != AMF3_OBJECT)
		{
			//RTMP_Log(//RTMP_LOGERROR, "AMF3 Object encapsulated in AMF stream does not start with AMF3_OBJECT!");
		}

		strInput++;
		iSize--;
	}

	ref = 0;
	len = AMF3ReadInteger(ref, strInput);
	strInput += len;
	iSize -= len;

	if ((ref & 1) == 0)
	{
		/* object reference, 0xxx */
		uint32_t objectIndex = (ref >> 1);

		//RTMP_Log(//RTMP_LOGDEBUG, "Object reference, index: %d", objectIndex);
	}
	else				/* object instance */
	{
		int32_t classRef = (ref >> 1);

		AMF3ClassDef cd = { { 0, 0 } };
		AMFObjectProperty prop;

		if ((classRef & 0x1) == 0)
		{
			/* class reference */
			uint32_t classIndex = (classRef >> 1);
			//RTMP_Log(//RTMP_LOGDEBUG, "Class reference: %d", classIndex);
		}
		else
		{
			int32_t classExtRef = (classRef >> 1);
			int i;

			cd.cd_externalizable = (classExtRef & 0x1) == 1;
			cd.cd_dynamic = ((classExtRef >> 1) & 0x1) == 1;

			cd.cd_num = classExtRef >> 2;

			/* class name */

			len = AMF3ReadString(cd.cd_name, strInput);
			iSize -= len;
			strInput += len;

			/*std::string str = className; */

			//RTMP_Log(//RTMP_LOGDEBUG,
			//	"Class name: %s, externalizable: %d, dynamic: %d, classMembers: %d",
			//	cd.cd_name.av_val, cd.cd_externalizable, cd.cd_dynamic,
			//	cd.cd_num);

			for (i = 0; i < cd.cd_num; i++)
			{
				AVal memberName = AV_empty;
				len = AMF3ReadString(memberName, strInput);
				//RTMP_Log(//RTMP_LOGDEBUG, "Member: %s", memberName.av_val);
				cd.AMF3CD_AddProp(&memberName);
				iSize -= len;
				strInput += len;
			}
		}

		/* add as referencable object */

		if (cd.cd_externalizable)
		{
			int nRes;
			AVal name = AVC("DEFAULT_ATTRIBUTE");

			//RTMP_Log(//RTMP_LOGDEBUG, "Externalizable, TODO check");

			nRes = prop.Decode(strInput, iSize, FALSE);
			if (nRes == -1)
			{
				//RTMP_Log(//RTMP_LOGDEBUG, "%s, failed to decode AMF3 property!",
				//	__FUNCTION__);
			}
			else
			{
				iSize -= nRes;
				strInput += nRes;
			}

			prop.SetName(name);
			AddProp(&prop);
		}
		else
		{
			int nRes, i;
			for (i = 0; i < cd.cd_num; i++)	/* non-dynamic */
			{
				nRes = prop.Decode(strInput, iSize, FALSE);
				if (nRes == -1)
				{
					//RTMP_Log(//RTMP_LOGDEBUG, "%s, failed to decode AMF3 property!",
					//	__FUNCTION__);
				}

				prop.SetName(*cd.AMF3CD_GetProp(i));
				AddProp(&prop);

				strInput += nRes;
				iSize -= nRes;
			}
			if (cd.cd_dynamic)
			{
				int len = 0;

				do
				{
					nRes = prop.Decode(strInput, iSize, TRUE);
					AddProp(&prop);

					strInput += nRes;
					iSize -= nRes;

					len = prop.p_name.av_len;
				} while (len > 0);
			}
		}
		//RTMP_Log(//RTMP_LOGDEBUG, "class object!");
	}
	return nOriginalSize - iSize;
}

/************************************************************************************************************
*	������prop׷�ӵ�obj��(���ʵ��);
*
************************************************************************************************************/
void AMFObject::AddProp(IN const AMFObjectProperty* pProp)
{
	if (!(o_num & 0x0f))
	{
		// �˴�����˼ÿ��һ��������16���ڴ�, ����17������׷��ʱ���ᴥ��������16���ڴ�;
		o_props = (AMFObjectProperty *)realloc(o_props, (o_num + 16) * sizeof(AMFObjectProperty));
	}

	memcpy(&o_props[o_num++], pProp, sizeof(AMFObjectProperty));
}

/************************************************************************************************************
*	��obj�ڵ��������Խ��������ʾ,���ڵ���;
*
************************************************************************************************************/
void AMFObject::Dump()
{
	//RTMP_Log(//RTMP_LOGDEBUG, "(object begin)");
	for (int i = 0; i < o_num; i++)
	{
		o_props[i].Dump();
	}
	//RTMP_Log(//RTMP_LOGDEBUG, "(object end)");
}

/************************************************************************************************************
*	��obj�ڵ��������Խ�������,����ͷ���������;
*
************************************************************************************************************/
void AMFObject::Reset()
{
	for (int i = 0; i < o_num; i++)
	{
		o_props[i].Reset();
	}
	free(o_props);
	o_props = NULL;
	o_num = 0;
}


/************************************************************************************************************
*	��ȡobj�ڵ���������;
*
************************************************************************************************************/
int AMFObject::GetPropCount()
{
	return o_num;
}

/************************************************************************************************************
*	��ȡobj�ڵ�ĳ������;
*
*	������nIndex���з���, ��nIndex<0 �����name����ɸѡ;
************************************************************************************************************/
AMFObjectProperty* AMFObject::GetProp(IN const AVal& strName, IN const int iIndex)
{
	if (iIndex >= 0)
	{
		if (iIndex < o_num)
		{
			return &o_props[iIndex];
		}
	}
	else
	{
		int n;
		for (n = 0; n < o_num; n++)
		{
			if (AVMATCH(&o_props[n].p_name, &strName))
			{
				return &o_props[n];
			}
		}
	}

	return (AMFObjectProperty*)&AMFProp_Invalid;
}

/* AMFObjectProperty */
/************************************************************************************************************
*	���ö������Ե�name;
*
************************************************************************************************************/
void AMFObjectProperty::SetName(IN const AVal& strName)
{
	p_name = strName;
}

/************************************************************************************************************
*	��ȡ�������Ե�name;
*
************************************************************************************************************/
void AMFObjectProperty::GetName(OUT AVal& strName)
{
	strName = p_name;
}

/************************************************************************************************************
*	���ö������Ե�����;
*
************************************************************************************************************/
void AMFObjectProperty::SetType(IN const AMFDataType& iType)
{
	p_type = iType;
}

/************************************************************************************************************
*	��ȡ�������Ե�����;
*
************************************************************************************************************/
AMFDataType AMFObjectProperty::GetType()
{
	return p_type;
}

/************************************************************************************************************
*	���ö������Ե���ֵ(double);
*
************************************************************************************************************/
void AMFObjectProperty::SetNumber(IN const double& dVal)
{
	p_vu.p_number = dVal;
}

/************************************************************************************************************
*	��ȡ�������Ե���ֵ(double);
*
************************************************************************************************************/
double AMFObjectProperty::GetNumber()
{
	return p_vu.p_number;
}

/************************************************************************************************************
*	���ö������Ե���ֵ(bool);
*
************************************************************************************************************/
void AMFObjectProperty::SetBoolean(IN const bool& bVal)
{
	p_vu.p_number = bVal ? 1 : 0;
}

/************************************************************************************************************
*	��ȡ�������Ե���ֵ(bool);
*
************************************************************************************************************/
bool AMFObjectProperty::GetBoolean()
{
	return (0 != (p_vu.p_number));
}

/************************************************************************************************************
*	���ö������Ե���ֵ(string);
*
************************************************************************************************************/
void AMFObjectProperty::SetString(IN const AVal& strVal)
{
	p_vu.p_aval = strVal;
}

/************************************************************************************************************
*	��ȡ�������Ե���ֵ(string);
*
************************************************************************************************************/
void AMFObjectProperty::GetString(IN AVal& strVal)
{
	strVal = p_vu.p_aval;
}

/************************************************************************************************************
*	���ö������Ե���ֵ(object);
*
************************************************************************************************************/
void AMFObjectProperty::SetObject(IN const AMFObject& objVal)
{
	p_vu.p_object = objVal;
}

/************************************************************************************************************
*	��ȡ�������Ե���ֵ(object);
*
************************************************************************************************************/
void AMFObjectProperty::GetObject(IN AMFObject& objVal)
{
	objVal = p_vu.p_object;
}

/************************************************************************************************************
*	�ж϶������Ե������Ƿ���Ч;
*
************************************************************************************************************/
int AMFObjectProperty::IsValid()
{
	return (AMF_INVALID != p_type);
}

/************************************************************************************************************
*	���룺 ���������prop;
*
************************************************************************************************************/
char* AMFObjectProperty::Encode(OUT char* strOutPut, OUT char* strOutEnd)
{
	if (p_type == AMF_INVALID)
	{
		return NULL;
	}

	if (p_type != AMF_NULL && strOutPut + p_name.av_len + 2 + 1 >= strOutEnd)
	{
		return NULL;
	}

	// ��������name,�����ֽڴ泤��;
	// ֮���Բ�ֱ�ӵ���EncodeString ����Ϊ����һ���ֽ�(��ʾ��������);
	if (p_type != AMF_NULL && p_name.av_len)
	{
		*strOutPut++ = p_name.av_len >> 8;
		*strOutPut++ = p_name.av_len & 0xff;
		memcpy(strOutPut, p_name.av_val, p_name.av_len);
		strOutPut += p_name.av_len;
	}

	// ��������value, ��ͬ���Ͳ�ͬ����;
	switch (p_type)
	{
	case AMF_NUMBER:
		{
			strOutPut = AMFObject::EncodeNumber(strOutPut, strOutEnd, p_vu.p_number);
		}
		break;
	case AMF_BOOLEAN:
		{
			strOutPut = AMFObject::EncodeBoolean(strOutPut, strOutEnd, p_vu.p_number != 0);
		}
		break;
	case AMF_STRING:
		{
			strOutPut = AMFObject::EncodeString(strOutPut, strOutEnd, p_vu.p_aval);
		}
		break;
	case AMF_NULL:
		{
			if (strOutPut + 1 >= strOutEnd)
			{
				return NULL;
			}
			*strOutPut++ = AMF_NULL;
		}
		break;
	case AMF_OBJECT:
		{
			strOutPut = p_vu.p_object.Encode(strOutPut, strOutEnd);
		}
		break;
	case AMF_ECMA_ARRAY:
		{
			strOutPut = p_vu.p_object.EncodeEcmaArray(strOutPut, strOutEnd);
		}
		break;
	case AMF_STRICT_ARRAY:
		{
			strOutPut = p_vu.p_object.EncodeArray(strOutPut, strOutEnd);
		}
		break;
	default:
		{
			//RTMP_Log(//RTMP_LOGERROR, "%s, invalid type. %d", __FUNCTION__, p_type);
			strOutPut = NULL;
		}
		break;
	};

	return strOutPut;
}

/************************************************************************************************************
*	���룺 pBuffer->prop;
*
************************************************************************************************************/
int AMFObjectProperty::Decode(IN const char* strInput, IN int& iSize, IN const bool& bDecodeName)
{
	int nOriginalSize = iSize;
	int nRes;

	p_name.av_len = 0;
	p_name.av_val = NULL;

	if (iSize == 0 || !strInput)
	{
		//RTMP_Log(//RTMP_LOGDEBUG, "%s: Empty buffer/no buffer pointer!", __FUNCTION__);
		return -1;
	}

	if (bDecodeName && iSize < 4)
	{
		/* at least name (length + at least 1 byte) and 1 byte of data */
		//RTMP_Log(//RTMP_LOGDEBUG, "%s: Not enough data for decoding with name, less than 4 bytes!", __FUNCTION__);
		return -1;
	}

	if (bDecodeName)
	{
		// ����������Ե�name;
		unsigned short nNameSize = AMFObject::DecodeInt16(strInput);
		if (nNameSize > iSize - 2)
		{
			//RTMP_Log(//RTMP_LOGDEBUG, "%s: Name size out of range: namesize (%d) > len (%d) - 2", __FUNCTION__, nNameSize, nSize);
			return -1;
		}

		AMFObject::DecodeString(p_name, strInput);
		iSize -= 2 + nNameSize;
		strInput += 2 + nNameSize;
	}

	if (iSize == 0)
	{
		return -1;
	}

	// ��ȡ��������;
	iSize--;
	p_type = (AMFDataType)*strInput++;
	switch (p_type)
	{
	case AMF_NUMBER:
	{
		if (iSize < 8)
		{
			return -1;
		}

		p_vu.p_number = AMFObject::DecodeNumber(strInput);
		iSize -= 8;
	}
	break;
	case AMF_BOOLEAN:
	{
		if (iSize < 1)
		{
			return -1;
		}

		p_vu.p_number = (double)AMFObject::DecodeBoolean(strInput);
		iSize--;
	}
	break;
	case AMF_STRING:
	{
		unsigned short nStringSize = AMFObject::DecodeInt16(strInput);
		if (iSize < (long)nStringSize + 2)
		{
			return -1;
		}
		AMFObject::DecodeString(p_vu.p_aval, strInput);
		iSize -= (2 + nStringSize);
	}
	break;
	case AMF_OBJECT:
	{
		int nRes = p_vu.p_object.Decode(strInput, iSize, TRUE);
		if (nRes == -1)
		{
			return -1;
		}
		iSize -= nRes;
	}
	break;
	case AMF_MOVIECLIP:
	{
		//RTMP_Log(//RTMP_LOGERROR, "AMF_MOVIECLIP reserved!");
		return -1;
	}
	break;
	case AMF_NULL:
	case AMF_UNDEFINED:
	case AMF_UNSUPPORTED:
	{
		p_type = AMF_NULL;
	}
	break;
	case AMF_REFERENCE:
	{
		//RTMP_Log(//RTMP_LOGERROR, "AMF_REFERENCE not supported!");
		return -1;
	}
	break;
	case AMF_ECMA_ARRAY:
	{
		iSize -= 4;
		/* next comes the rest, mixed array has a final 0x000009 mark and names, so its an object */
		nRes = p_vu.p_object.Decode(strInput + 4, iSize, TRUE);
		if (nRes == -1)
		{
			return -1;
		}
		iSize -= nRes;
	}
	break;
	case AMF_OBJECT_END:
	{
		return -1;
	}
	break;
	case AMF_STRICT_ARRAY:
	{
		unsigned int nArrayLen = AMFObject::DecodeInt32(strInput);
		iSize -= 4;
		nRes = p_vu.p_object.DecodeArray(strInput + 4, iSize, nArrayLen, FALSE);
		if (nRes == -1)
		{
			return -1;
		}
		iSize -= nRes;
	}
	break;
	case AMF_DATE:
	{
		//RTMP_Log(//RTMP_LOGDEBUG, "AMF_DATE");
		if (iSize < 10)
		{
			return -1;
		}
		p_vu.p_number = AMFObject::DecodeNumber(strInput);
		p_UTCoffset = AMFObject::DecodeInt16(strInput + 8);

		iSize -= 10;
	}
	break;
	case AMF_LONG_STRING:
	case AMF_XML_DOC:
	{
		unsigned int nStringSize = AMFObject::DecodeInt32(strInput);
		if (iSize < (long)nStringSize + 4)
		{
			return -1;
		}
		AMFObject::DecodeLongString(p_vu.p_aval, strInput);
		iSize -= (4 + nStringSize);
		if (p_type == AMF_LONG_STRING)
		{
			p_type = AMF_STRING;
		}
	}
	break;
	case AMF_RECORDSET:
	{
		//RTMP_Log(//RTMP_LOGERROR, "AMF_RECORDSET reserved!");
		return -1;
	}
	break;
	case AMF_TYPED_OBJECT:
	{
		//RTMP_Log(//RTMP_LOGERROR, "AMF_TYPED_OBJECT not supported!");
		return -1;
	}
	break;
	case AMF_AVMPLUS:
	{
		int nRes = p_vu.p_object.Decode_AMF3(strInput, iSize, TRUE);
		if (nRes == -1)
		{
			return -1;
		}
		iSize -= nRes;
		p_type = AMF_OBJECT;
	}
	break;
	default:
	{
		//RTMP_Log(//RTMP_LOGDEBUG, "%s - unknown datatype 0x%02x, @%p", __FUNCTION__, p_type, pBuffer - 1);
		return -1;
	}
	break;
	}

	return nOriginalSize - iSize;
}

/************************************************************************************************************
*	���룺 ���������;
*
************************************************************************************************************/
int AMFObjectProperty::Decode_AMF3(IN const char* strInput, IN int& iSize, IN const bool& bDecodeName)
{
	int nOriginalSize = iSize;
	AMF3DataType type;

	p_name.av_len = 0;
	p_name.av_val = NULL;

	if (iSize == 0 || !strInput)
	{
		//RTMP_Log(//RTMP_LOGDEBUG, "empty buffer/no buffer pointer!");
		return -1;
	}

	/* decode name */
	if (bDecodeName)
	{
		AVal name = AV_empty;
		int nRes = AMFObject::AMF3ReadString(name, strInput);

		if (name.av_len <= 0)
			return nRes;

		p_name = name;
		strInput += nRes;
		iSize -= nRes;
	}

	/* decode */
	type = (AMF3DataType)*strInput++;
	iSize--;

	switch (type)
	{
	case AMF3_UNDEFINED:
	case AMF3_NULL:
		p_type = AMF_NULL;
		break;
	case AMF3_FALSE:
		p_type = AMF_BOOLEAN;
		p_vu.p_number = 0.0;
		break;
	case AMF3_TRUE:
		p_type = AMF_BOOLEAN;
		p_vu.p_number = 1.0;
		break;
	case AMF3_INTEGER:
	{
		int32_t res = 0;
		int len = AMFObject::AMF3ReadInteger(res, strInput);
		p_vu.p_number = (double)res;
		p_type = AMF_NUMBER;
		iSize -= len;
		break;
	}
	case AMF3_DOUBLE:
		if (iSize < 8)
			return -1;
		p_vu.p_number = AMFObject::DecodeNumber(strInput);
		p_type = AMF_NUMBER;
		iSize -= 8;
		break;
	case AMF3_STRING:
	case AMF3_XML_DOC:
	case AMF3_XML:
	{
		int len = AMFObject::AMF3ReadString(p_vu.p_aval, strInput);
		p_type = AMF_STRING;
		iSize -= len;
		break;
	}
	case AMF3_DATE:
	{
		int32_t res = 0;
		int len = AMFObject::AMF3ReadInteger(res, strInput);

		iSize -= len;
		strInput += len;

		if ((res & 0x1) == 0)
		{
			/* reference */
			uint32_t nIndex = (res >> 1);
			//RTMP_Log(//RTMP_LOGDEBUG, "AMF3_DATE reference: %d, not supported!", nIndex);
		}
		else
		{
			if (iSize < 8)
				return -1;

			p_vu.p_number = AMFObject::DecodeNumber(strInput);
			iSize -= 8;
			p_type = AMF_NUMBER;
		}
		break;
	}
	case AMF3_OBJECT:
	{
		int nRes = p_vu.p_object.Decode_AMF3(strInput, iSize, TRUE);
		if (nRes == -1)
			return -1;
		iSize -= nRes;
		p_type = AMF_OBJECT;
		break;
	}
	case AMF3_ARRAY:
	case AMF3_BYTE_ARRAY:
	default:
		//RTMP_Log(//RTMP_LOGDEBUG, "%s - AMF3 unknown/unsupported datatype 0x%02x, @%p",
		//	__FUNCTION__, (unsigned char)(*pBuffer), pBuffer);
		return -1;
	}

	return nOriginalSize - iSize;
}

/************************************************************************************************************
*	������prop���������ʾ,���ڵ���;
*
************************************************************************************************************/
void AMFObjectProperty::Dump()
{
	char strRes[256];
	char str[256];
	AVal name;

	if (p_type == AMF_INVALID)
	{
		//RTMP_Log(//RTMP_LOGDEBUG, "Property: INVALID");
		return;
	}

	if (p_type == AMF_NULL)
	{
		//RTMP_Log(//RTMP_LOGDEBUG, "Property: NULL");
		return;
	}

	if (p_name.av_len)
	{
		name = p_name;
	}
	else
	{
		name.av_val = "no-name.";
		name.av_len = sizeof("no-name.") - 1;
	}
	if (name.av_len > 18)
	{
		name.av_len = 18;
	}
	snprintf(strRes, 255, "Name: %18.*s, ", name.av_len, name.av_val);

	switch (p_type)
	{
	case AMF_OBJECT:
	{
		//RTMP_Log(//RTMP_LOGDEBUG, "Property: <%sOBJECT>", strRes);
		p_vu.p_object.Dump();
		return;
	}
	break;
	case AMF_ECMA_ARRAY:
	{
		//RTMP_Log(//RTMP_LOGDEBUG, "Property: <%sECMA_ARRAY>", strRes);
		p_vu.p_object.Dump();
		return;
	}
	break;
	case AMF_STRICT_ARRAY:
	{
		//RTMP_Log(//RTMP_LOGDEBUG, "Property: <%sSTRICT_ARRAY>", strRes);
		p_vu.p_object.Dump();
		return;
	}
	break;
	case AMF_NUMBER:
		snprintf(str, 255, "NUMBER:\t%.2f", p_vu.p_number);
		break;
	case AMF_BOOLEAN:
		snprintf(str, 255, "BOOLEAN:\t%s", p_vu.p_number != 0.0 ? "TRUE" : "FALSE");
		break;
	case AMF_STRING:
		snprintf(str, 255, "STRING:\t%.*s", p_vu.p_aval.av_len, p_vu.p_aval.av_val);
		break;
	case AMF_DATE:
		snprintf(str, 255, "DATE:\ttimestamp: %.2f, UTC offset: %d", p_vu.p_number, p_UTCoffset);
		break;
	default:
		snprintf(str, 255, "INVALID TYPE 0x%02x", (unsigned char)p_type);
		break;
	}

	//RTMP_Log(//RTMP_LOGDEBUG, "Property: <%s%s>", strRes, str);
}

/************************************************************************************************************
*	����prop������;
*
************************************************************************************************************/
void AMFObjectProperty::Reset()
{
	if (p_type == AMF_OBJECT || p_type == AMF_ECMA_ARRAY || p_type == AMF_STRICT_ARRAY)
	{
		p_vu.p_object.Reset();
	}
	else
	{
		p_vu.p_aval.av_len = 0;
		p_vu.p_aval.av_val = NULL;
	}
	p_type = AMF_INVALID;
}

/* AMF3ClassDef */
/************************************************************************************************************
*	���ַ���prop׷�ӵ�cd�ڵ��ַ���������(���);
*
************************************************************************************************************/
void AMF3ClassDef::AMF3CD_AddProp(AVal * pProp)
{
	if (!(cd_num & 0x0f))
	{
		cd_props = (AVal *)realloc(cd_props, (cd_num + 16) * sizeof(AVal));
	}
	cd_props[cd_num++] = *pProp;
}

/************************************************************************************************************
*	��ȡcd�����ڵ��ַ��������ڵĵ�nIndex���ַ���;
*
************************************************************************************************************/
AVal * AMF3ClassDef::AMF3CD_GetProp(int iIndex)
{
	if (iIndex >= cd_num)
	{
		return (AVal*)&AV_empty;
	}
	return &cd_props[iIndex];
}