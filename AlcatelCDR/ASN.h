#include <stdio.h>
#include <string.h>
#include <vector>
#include <string>

using namespace std;

#define MAX_MESSAGE_SIZE					65535
#define TAG_INTEGER							2
#define TAG_OCTET_STRING					4
#define TAG_ENUMERATED						10
#define TAG_BOOLEAN							1

#define TAG_MOCALLRECORD					1
#define TAG_UNBIND_REQUEST					2
#define TAG_AUTHENTICATION_SIMPLE			0
#define TAG_BIND_RESPONSE					1
#define TAG_EXTENDED_RESPONSE				24
#define TAG_SEARCH_REQUEST					3
#define TAG_SEARCH_RESULT_ENTRY				4
#define TAG_SEARCH_RESULT_DONE				5
#define TAG_FILTER_PRESENT					7
#define TAG_ADD_REQUEST						8
#define TAG_ADD_RESPONSE					9
#define TAG_DEL_REQUEST						10
#define TAG_DEL_RESPONSE					11
#define TAG_MODIFY_REQUEST					6
#define TAG_MODIFY_RESPONSE					7

enum _TagClass {
	tcUniversal = 0,
	tcApplication = 1,
	tcContextSpecific = 2,
	tcPrivate = 3
};

class ASNType
{
protected:
	_TagClass tagClass;
	int tagID;
	bool constructed;
	vector<unsigned char> data;
	vector<ASNType> members;
	
	int encodeTag(unsigned char* buffer);
	int encodeLength(unsigned long,unsigned char*);
	int decodeTag(const unsigned char* buffer,int bufferLen);
	int decodeLen(const unsigned char* buffer,unsigned long* dataLen);
public:
	ASNType();
	ASNType(_TagClass _tagClass,int _tagID,bool _constructed,const unsigned char* pdata,int _len);
	ASNType(bool _constructed,bool _setOrSequence);
	~ASNType();
	void initialize(_TagClass _tagClass,int _tagID,bool _constructed,const unsigned char* pdata,int _len);
	void initialize(_TagClass _tagClass,int _tagID,bool _constructed,vector<unsigned char>& _data);
	virtual int encode(unsigned char* buffer);
	virtual unsigned long decode(const unsigned char* buffer, unsigned long bufferLen);
	void add(ASNType);
	ASNType& getMember(int);
	vector<unsigned char>& getData();
	
	int getTagID();
	bool IsConstructed();
	
};

class ASNInteger : public ASNType 
{
protected:
	unsigned long value;
	void encode_data(int requested_len=0);
public:
	ASNInteger(unsigned long);
	ASNInteger(_TagClass _tagClass, int _tagID, unsigned long _value);
	ASNInteger(_TagClass _tagClass, int _tagID, unsigned long _value, short len);	// len in bytes
	ASNInteger(ASNType&);
	unsigned long getValue();
};

class ASNOctetString : public ASNType
{
protected:
	string value;
public:
	ASNOctetString();
	ASNOctetString(_TagClass,int,string);
	ASNOctetString(string);
	ASNOctetString(ASNType&);
	virtual int encode(unsigned char* buffer);
	const string& getValue();
};

class ASNTBCDString : public ASNOctetString
{
public:
	ASNTBCDString(_TagClass _tagClass, int _tagID, string _value, int minimal_size=0, bool _switchDigits=true, bool _addressString=false);
};


class ASN_overflow {};
class ASN_decode_fail {};
class ASN_type_mismatch {};
class ASN_index_out_of_bound {};

class CallRecord
{
protected:
	unsigned short callRecordType;
	unsigned long billingRecordID;
	unsigned short recordType;
	unsigned short transactionType;
	string servedIMEI;
	string servedIMSI;
	string servedMSISDN;
	string startOfChrDate;
	string startOfChrTime;
	int callDuration;
	unsigned int mscID;
	unsigned int locationAreaCode;
	unsigned int cellID;
	string otherParty;
	unsigned short serviceType;
	unsigned short serviceCode;
	string mscAddress;

	ASNType asnObject;
public:
	CallRecord(unsigned short _callRecordType, unsigned long _billingRecordID,	unsigned short _transactionType, string _servedIMEI, string _servedIMSI, string _servedMSISDN, string _startOfChrDate, string _startOfChrTime,
		unsigned int _callDuration,	unsigned int _mscID, unsigned int _locationAreaCode, unsigned int _cellID, string _otherParty, unsigned short _serviceType, 
		unsigned short _serviceCode, string _mscAddress) :
		callRecordType(_callRecordType), billingRecordID(_billingRecordID), recordType(0 /*singleTypeA*/), transactionType(_transactionType), servedIMEI(_servedIMEI), servedIMSI(_servedIMSI), servedMSISDN(_servedMSISDN), 
			startOfChrDate(_startOfChrDate), startOfChrTime(_startOfChrTime), callDuration(_callDuration), mscID(_mscID), locationAreaCode(_locationAreaCode), cellID(_cellID), otherParty(_otherParty), 
			serviceType(_serviceType), serviceCode(_serviceCode), mscAddress(_mscAddress), asnObject(tcPrivate, _callRecordType /* MOCallRecord tag */, true, NULL, 0) {}

	ASNType& getASNObject();
};
