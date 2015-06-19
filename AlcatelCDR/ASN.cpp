#include "ASN.h"

ASNType::ASNType() : tagClass(tcUniversal), tagID(0), constructed(false)
{
}

void ASNType::initialize(_TagClass _tagClass,int _tagID,bool _constructed,const unsigned char* pdata,int _len)
{
	tagClass=_tagClass;
	tagID=_tagID;
	constructed=_constructed;
	if(pdata)
		data.assign(pdata,pdata+_len);
}

void ASNType::initialize(_TagClass _tagClass,int _tagID,bool _constructed,vector<unsigned char>& _data)
{
	tagClass=_tagClass;
	tagID=_tagID;
	constructed=_constructed;

	data=_data;
}

ASNType::ASNType(_TagClass _tagClass,int _tagID,bool _constructed,const unsigned char* pdata,int _len)
{
	initialize(_tagClass, _tagID, _constructed, pdata, _len);
}

ASNType::ASNType(bool _constructed,bool _setOrSequence=false) : tagClass(tcUniversal)
{
	if(_constructed) {
		if(_setOrSequence)
			tagID = 0x11;
		else
			tagID = 0x10;
		constructed=true;
	}
	else {
		tagID=0;
		constructed=false;
	}
}

ASNType::~ASNType()
{
	//if(constructed && dynamic_members)
	//	for(auto& it=members.begin(); it != members.end(); it++) 
	//		delete (*it);
}

int ASNType::encodeTag(unsigned char* buffer)
{
	int len=0;
	char tempBuffer[16];
	int tempLen=0;

	buffer[0]=0;
	buffer[0] |= tagClass << 6;
	if(constructed)
		buffer[0] |= 0x20;
	len++;
	if(tagID < 0x1F) 
		buffer[0] |= tagID;
	else {
		// ID тэга кодируетс€ несколькими октетами, подробнее http://habrahabr.ru/post/150757/. “эг раскладываетс€ по основанию 128 и кодируетс€ 7-битными част€ми
		buffer[0] |= 0x1F;
		tempLen=0;
		int tagLeft=tagID;
		while( tagLeft>0 ) {
			tempBuffer[15-tempLen] = tagLeft & 0x7F;
			tempBuffer[15-tempLen] |= 0x80;		// установим бит 7 в 1, что значит продолжение тэга
			tagLeft >>= 7;
			tempLen++;
		}
		tempBuffer[15] &= 0x7F;	// у завершающего октета 7-ой бит должен быть обнулен
		memcpy(buffer+len,tempBuffer+16-tempLen,tempLen);
	}

	return len+tempLen;
}

int ASNType::encodeLength(unsigned long lenToEncode,unsigned char* buffer)
{
	int len=0;
	char tempBuffer[16];
	unsigned char tempLen=0;

	if( lenToEncode <= 128 )
		buffer[len++] = (unsigned char) lenToEncode;
	else {
		tempLen=0;
		int lenLeft=lenToEncode;
		while( lenLeft>0 ) {
			tempBuffer[15-tempLen] = lenLeft & 0xFF;
			lenLeft >>= 8;
			if(tempLen++ > 16)
				throw ASN_overflow();
		}
		buffer[len++]=0x80 | tempLen; 
		memcpy(buffer+len,tempBuffer+16-tempLen,tempLen);
	}

	return len+tempLen;
}

int ASNType::encode(unsigned char* buffer)
{
	if( !constructed) {
		int len=encodeTag(buffer);
		len += encodeLength(data.size(), buffer+len);
		memcpy(buffer+len,data.data(),data.size());

		return len+data.size();
	}
	else {
		int tempLen=0;
		unsigned char tempBuffer[MAX_MESSAGE_SIZE];
		for(auto it=members.begin(); it != members.end(); it++) {
			/*ASNSequence* pSeq = dynamic_cast<ASNSequence*>(*it);
			if(pSeq)
				tempLen += pSeq->encode(tempBuffer + tempLen);
			else*/
				tempLen += (*it).encode(tempBuffer + tempLen);
		}

		int len=encodeTag(buffer);
		len += encodeLength(tempLen, buffer+len);

		memcpy(buffer+len,tempBuffer,tempLen);

		return len + tempLen;
	}
}

int ASNType::decodeTag(const unsigned char* buffer,int bufferLen)
{
	int i=1;
	tagClass = static_cast<_TagClass>((buffer[0] & 0xC0) >> 6);
	constructed = (buffer[0] & 0x20) > 0;
	if((buffer[0] & 0x1F) < 0x1F)
		tagID = buffer[0] & 0x1F;
	else {
		tagID = 0;
		// ID тэга закодирован несколькими октетами, подробнее см. encodeTag
		do {
			tagID <<= 7;
			tagID |= buffer[i] & 0x7F;
			if(i >= bufferLen) 
				throw ASN_decode_fail();
		} while(buffer[i++] & 0x80);
	}
	return i;
}

int ASNType::decodeLen(const unsigned char* buffer,unsigned long* dataLen)
{
	*dataLen=0;
	int i=1;

	if(!(buffer[0] & 0x80) )
		*dataLen = buffer[0];
	else {
		// ƒлина данных закодирована несколькими октетами, подробнее см. encodeLen
		int nOctets = buffer[0] & 0x7F;
		if(nOctets > sizeof(*dataLen))
			throw ASN_decode_fail();
		for(; i<nOctets+1; i++) {
			*dataLen <<= 8;
			*dataLen |= buffer[i];
		}
	}
	return i;
}

unsigned long ASNType::decode(const unsigned char* buffer, unsigned long bufferLen)
	// функци€ формирует объект класса из переданного буфера
{
	int len=decodeTag(buffer, bufferLen);
	unsigned long dataLen;		

	if( !constructed ) {
		len += decodeLen(buffer+len, &dataLen);

		data.assign(buffer+len,buffer+len+dataLen);
		return len+dataLen;
	}
	else {
		len += decodeLen(buffer+len, &dataLen);

		//data.assign(buffer+len,buffer+len+dataLen);
		unsigned long nextLen=len;
	
		while(nextLen < dataLen) {
			ASNType asnNextElem;
			nextLen += asnNextElem.decode(buffer+nextLen, dataLen);
			add(asnNextElem);
		}

		return nextLen;
	}
}


void ASNType::add(ASNType elem)
{
	if(!constructed)
		throw ASN_type_mismatch();
	members.push_back(elem);
}

ASNType& ASNType::getMember(int index)
{
	if(!constructed)
		throw ASN_type_mismatch(); 

	return members.at(index);
}

vector<unsigned char>& ASNType::getData()
{
	if(constructed)
		throw ASN_type_mismatch(); 

	return data;
}

int ASNType::getTagID()
{
	return tagID;
}

bool ASNType::IsConstructed()
{
	return constructed;
}
//-------------------------------------

ASNInteger::ASNInteger(unsigned long _value) : value(_value)
{
	tagClass=tcUniversal;
	tagID=TAG_INTEGER;
	constructed=false;

	//initialize(tcUniversal, TAG_INTEGER, false, buffer+16-len, len);
	encode_data();
}

ASNInteger::ASNInteger(_TagClass _tagClass, int _tagID, unsigned long _value) : value(_value)
{
	tagClass = _tagClass;
	tagID=_tagID;
	constructed=false;

	//initialize(tcUniversal, TAG_INTEGER, false, buffer+16-len, len);
	encode_data();
}

ASNInteger::ASNInteger(_TagClass _tagClass, int _tagID, unsigned long _value, short len) : value(_value) 
{
	tagClass = _tagClass;
	tagID=_tagID;
	constructed=false;

	encode_data(len);
}

void ASNInteger::encode_data(int requested_len) // default requested len=0 so we set len to real number of bytes
{
	unsigned char buffer[16];
	int len=0;

	memset(buffer, 0, 16);
	if(value == 0) {
		// buffer is already set to 0
		len = requested_len>0 ? requested_len : 1;
	}
	else {
		while(value>0) {
			buffer[15-len] = value & 0xFF;
			value >>= 8;
			if(++len > 16)
				throw "ASNInteger overflow: value is too big to encode";
		}
		if(requested_len>0)
			len = requested_len;
	}

	data.assign(buffer+16-len, buffer+16);
}

ASNInteger::ASNInteger(ASNType& pObject) 
	// декодирующий конструктор: создает объект из существующего объекта ASNType, использу€ его буфер data и заполн€€ по его содержимому value
{
	if(pObject.IsConstructed())
		throw ASN_type_mismatch();

	initialize(tcUniversal, TAG_INTEGER, false, pObject.getData());

	value=0;
	for(unsigned int i=0; i<data.size(); i++) {
		value <<= 8;
		value |= data[i];
	}
}

unsigned long ASNInteger::getValue()
{
	return value;
}



//void ASNInteger::setValue(unsigned long _value);
//{
//	char buffer[16];
//	int len=0;
//
//	if(value == 0) {
//		buffer[0]=0;
//		initialize(tcUniversal, TAG_INTEGER, false, buffer, 1);
//		return;
//	}
//
//	while(value>0) {
//		buffer[15-len] = value & 0xFF;
//		value >>= 8;
//		if(++len > 16)
//			throw ASN_overflow();
//	}
//}
//------------------------------------------------------

ASNOctetString::ASNOctetString() : ASNType() 
{
	tagClass=tcUniversal;
	tagID = TAG_OCTET_STRING;
	constructed=false;
}

//ASNOctetString::ASNOctetString(string _str,_TagClass _tagClass=tcUniversal, int _tagID=0x04)
//{
//	tagClass=_tagClass;
//	tagID = _tagID;
//	constructed=false;
//
//	data.assign(_str.data(),_str.data()+_str.size());	// null-terminator не копируетс€
//}

ASNOctetString::ASNOctetString(string _str) 
{
	tagClass=tcUniversal;
	tagID = TAG_OCTET_STRING;
	constructed=false;

	value = _str;
	data.assign(value.data(),value.data()+value.size());	// null-terminator не копируетс€
}

ASNOctetString::ASNOctetString(_TagClass _tagClass,int _tagID,string _str)
{
	tagClass=_tagClass;
	tagID = _tagID;
	constructed=false;

	value = _str;
	data.assign(value.data(),value.data()+value.size());	// null-terminator не копируетс€
}

ASNOctetString::ASNOctetString(ASNType& pObject)
{
	if(pObject.IsConstructed())
		throw ASN_type_mismatch();

	initialize(tcUniversal, TAG_OCTET_STRING, false, pObject.getData());
	if(data.size()>0) 
		value.assign((const char*)data.data(), data.size());
	
}

int ASNOctetString::encode(unsigned char* buffer)
{
	int len=encodeTag(buffer);
	len += encodeLength(data.size(), buffer+len);

	memcpy(buffer+len,data.data(),data.size());
	return len + data.size();
}

const string& ASNOctetString::getValue()
{
	return value;
}

//---------------------------------------------

ASNTBCDString::ASNTBCDString(_TagClass _tagClass, int _tagID, string _value, int minimal_size, bool switchDigits, bool addressString)
{
	tagClass=_tagClass;
	tagID = _tagID;
	constructed=false;

	value = _value;

	if(value.length() > 0) {
		if(_value[0] == '+') {
			if(addressString) 
				// coding AddressString (for example, Other Party field)
				data.push_back( 0x91 );	// international number, numbering plan=E.164
			
			// delete leading + from number
			_value.erase( 0, 1 );
		}
		else
			if(addressString) 
				data.push_back( 0x81 ); // unknown type of number, numbering plan=E.164

		for (unsigned int i = 0; i < _value.length(); i += 2) {
			if(i < _value.length() - 1) 
				if( switchDigits )	
					data.push_back( _value[i]-'0' | ((_value[i+1]-'0')<<4) );
				else
					data.push_back( _value[i+1]-'0' | ((_value[i]-'0')<<4) );
			else
				if( switchDigits )	
					data.push_back( _value[i]-'0' | 0xF0 );
				else
					data.push_back( (_value[i]-'0')<<4 | 0x0F );	// add filler 'F'
		}
	}

	while( data.size() < (unsigned int) minimal_size )  data.push_back(0xFF);		// fill to requested size with F fillers
	
}

//---------------------------------------------

ASNType& CallRecord::getASNObject()
{
	/*ASNInteger asnLdapVersion(ldap_version);
	LDAPString asnUsername(username);
	ASNOctetString asnAuthentication(tcContextSpecific,TAG_AUTHENTICATION_SIMPLE,password);
	asnObject.add(asnLdapVersion);
	asnObject.add(asnUsername);
	asnObject.add(asnAuthentication);*/
	
	/*ASNInteger asnBillingRecordID*/ asnObject.add( ASNInteger(tcPrivate, 99, billingRecordID, 8 /*requested length*/)); // BillingRecordID
	asnObject.add( ASNInteger (recordType));
	asnObject.add( ASNInteger (transactionType));
	
	if( servedIMEI.length()>0 )
		asnObject.add( ASNTBCDString (tcPrivate, 8, servedIMEI, 8 /*minimal_size*/)); 
	if( servedIMSI.length()>0 )
		asnObject.add( ASNTBCDString (tcPrivate, 6, servedIMSI, 8));
	if( servedMSISDN.length()>0 )
		asnObject.add( ASNTBCDString (tcPrivate, 7, servedMSISDN, 8));
	
	ASNType asnChargingTimeData ( tcPrivate, 14, true, NULL, 0);	// sequence
	ASNTBCDString asnStartOfChrDate (tcPrivate, 19, startOfChrDate, 0, false /*no digit switching*/);
	ASNType asnStartOfChrTimeSet (tcPrivate, 20, true, NULL, 0);	// set of timestamp
	ASNTBCDString asnStartOfChrTime (tcUniversal, TAG_OCTET_STRING, startOfChrTime, 0, false /*no digit switching*/);
	asnStartOfChrTimeSet.add(asnStartOfChrTime);
	
	asnChargingTimeData.add(asnStartOfChrDate);
	asnChargingTimeData.add(asnStartOfChrTimeSet);
	if(callDuration > -1 )	// -1 is set for SMS, so we don't add it to sequence
		asnChargingTimeData.add(ASNInteger(tcPrivate, 17, callDuration));
	asnObject.add(asnChargingTimeData);

	asnObject.add( ASNInteger ( 0 ) );	// time quality=certain
	asnObject.add( ASNInteger ( tcPrivate, 10, mscID<<2 ));		// MSC Identification (Point Code shifted 2 bits to the left)
	asnObject.add( ASNInteger ( tcPrivate, 12, locationAreaCode));
	asnObject.add( ASNInteger ( tcPrivate, 15, cellID));

	if(otherParty.length()>0)
		asnObject.add( ASNTBCDString(tcPrivate, 9, otherParty, 12 /*minimal_size*/, true /*switchDigits*/, true /*addressString*/));

	if(serviceType == 0)
		// TeleService
		asnObject.add(ASNInteger (tcPrivate, 21, serviceCode));
	else
		// BearerService
		asnObject.add(ASNInteger (tcPrivate, 22, serviceCode));

	asnObject.add( ASNTBCDString(tcPrivate, 96, mscAddress, 0 /*minimal_size*/, true /*switchDigits*/, true /*addressString*/ ));

	
	
	return asnObject;
}