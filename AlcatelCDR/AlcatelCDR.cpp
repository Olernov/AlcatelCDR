// AlcatelCDR.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "ASN.h"

#define OTL_ORA11G
#define OTL_STL
#include "otlv4.h"
//extern "C" {
//#include "ncftp.h"
//}

extern "C" int ncftp_main(int argc, char **argv, char* result);

ofstream ofsLog;
otl_connect otlLogConnect;

void log(string message)
{
	time_t t = time(0);   // get time now
    struct tm * now = localtime( & t );
	
	(ofsLog.is_open() ? ofsLog : cout) << now->tm_mday << '.' << (now->tm_mon + 1) << '.' << (now->tm_year + 1900) << ' ' << now->tm_hour << ':' << now->tm_min << ':' << now->tm_sec << ' ' << message << endl;
}

void logToDB(short msgType, string msgText)
{
	otl_stream otlLog;
	
	if(msgText.length() > 2048)
		msgText = msgText.substr(0, 2048);
	try {
		otlLog.open(1, "insert into BILLING.TAPWRITER_LOG (datetime, msg_type, msg_text) values (sysdate, :msg_type/*short*/, :msg_text /*char[2048]*/)", otlLogConnect);
		otlLog << msgType << msgText;
	}
	catch (otl_exception &otlEx) {
		string msg = "Unable to write log message to DB: ";
		msg += msgText;
		log( msg );
		log( (char*) otlEx.msg );
		if( strlen(otlEx.stm_text) > 0 )
			log( (char*) otlEx.stm_text ); // log SQL that caused the error
		if( strlen(otlEx.var_info) > 0 )
			log( (char*) otlEx.var_info ); // log the variable that caused the error
	}
			
}


int assign_integer_option(string _name, string _value, long& param, long minValid, long maxValid)
{
	size_t stoi_idx;
	try {
		param = stoi(_value, &stoi_idx);
	}
	catch(...) {
		log("Wrong value for " + _name + " given in ini-file.");
		return -1;
	}
	if(stoi_idx < _value.length()) {
		log("Wrong value for " + _name + " given in ini-file.");
		return -1;
	}
	if(param<minValid || param>maxValid) {
		log("Wrong value for " + _name + " given in ini-file. Valid range is from " + to_string((long long)minValid) + " to " + to_string((long long) maxValid));
		return -1;
	}
	
	return 0;
}


int main(int argc, char* argv[])
{
	// Default initial settings
	long maxAllowedFileSize = 10000000; // near 10 megabytes
	string connectString;
	long pastLookupInterval = 90;		
	string ftpServer;
	string ftpUsername;
	string ftpPassword;
	string ftpPort;
	string ftpDirectory;

	ofsLog.open("AlcatelCDR.log", ofstream::app);

	// Reading initial settings from file
	string line;
	string option_name;
	string option_value;
	size_t pos, delim_pos, comment_pos;
	ifstream ifsSettings ("AlcatelCDR.ini", ifstream::in);
	if (!ifsSettings.is_open())	{
		log("Unable to open ini-file AlcatelCDR.ini");
		exit(1);
	}

	while ( getline (ifsSettings, line) )
	{
		size_t pos = line.find_first_not_of(" \t\r\n");
		if( pos != string::npos )
			if(line[pos] == '#' || line[pos] == '\0')
				continue;
		size_t delim_pos = line.find_first_of(" \t=", pos);
		if (delim_pos != string::npos) {
			string option_name = line.substr(pos, delim_pos-pos);
			transform(option_name.begin(), option_name.end(), option_name.begin(), ::toupper);

			size_t value_pos = line.find_first_not_of(" \t=", delim_pos);
			string option_value = line.substr(value_pos);
			size_t comment_pos = option_value.find_first_of(" \t#");
			if( comment_pos != string::npos )
				option_value = option_value.substr(0, comment_pos);

			if( option_name.compare( "CONNECT_STRING" ) == 0 )
				connectString = option_value;
			if( option_name.compare( "MAX_TAP_FILE_SIZE" ) == 0 ) 
				if ( assign_integer_option(option_name, option_value, maxAllowedFileSize, 1000, 1000000000) != 0 )
					exit(1);
			if( option_name.compare( "PAST_LOOKUP_INTERVAL" ) == 0 ) {
				if ( assign_integer_option(option_name, option_value, pastLookupInterval, 1, 365) != 0 )
					exit(1);
			}
			if( option_name.compare( "FTP_SERVER" ) == 0  )
				ftpServer = option_value;
			if( option_name.compare( "FTP_USERNAME" ) == 0  )
				ftpUsername = option_value;
			if( option_name.compare( "FTP_PASSWORD" ) == 0  )
				ftpPassword = option_value;
			if (option_name.compare("FTP_PORT") == 0)
				ftpPort = option_value;
			if( option_name.compare( "FTP_DIRECTORY" ) == 0  )
				ftpDirectory = option_value;
		}
	}
	ifsSettings.close();

	

	if(connectString.length() == 0) {
		log("Connect string to DB is not found in ini-file. Exiting.");
		exit(1);
	}

	log("---- Alcatel TAP writing service started ----");
	log("Init params: CONNECT_STRING: " + connectString + ", MAX_TAP_FILE_SIZE: " + to_string( static_cast<unsigned long long> (maxAllowedFileSize)) + ", PAST_LOOKUP_INTERVAL: " + to_string( static_cast<unsigned long long> (pastLookupInterval)) +
		", FTP_SERVER: " + ftpServer + ", FTP_USERNAME: " + ftpUsername + ", FTP_PASSWORD: " + ftpPassword + ", FTP_PORT: " + ftpPort + ", FTP_DIRECTORY: " + ftpDirectory);
	
	otl_connect otlConnect;

	try {
		otlConnect.rlogon(connectString.c_str());	
		otlLogConnect.rlogon(connectString.c_str());	
	}
	catch (otl_exception &otlEx) {
		log( "Unable to connect to DB:" );
		log( (char*) otlEx.msg );
		if( strlen(otlEx.stm_text) > 0 )
			log( (char*) otlEx.stm_text ); // log SQL that caused the error
		if( strlen(otlEx.var_info) > 0 )
			log( (char*) otlEx.var_info ); // log the variable that caused the error
		log("---- Alcatel TAP writing service finished with errors, see DB log----");
		if(ofsLog.is_open()) ofsLog.close();
		exit(1);
	}

	logToDB(0, "---- Alcatel TAP writing service started ----");

	otl_nocommit_stream otlStream;
	otl_nocommit_stream otlUpdate;
	long file_id;
	string filename;
	
	try {
		otlStream.open(1  /*stream buffer size in logical rows*/, "begin :id/*long,out*/ := Billing.TAP.RegisterFile(:fn/*char[100],out*/);  end;", otlConnect);
		otlStream >> file_id >> filename;
		otlStream.close();

		otlStream.open( 1000, // stream buffer size in logical rows
			"select record_rowid /*char*/, status /*long*/, transaction_type /*short*/, billrec_id /*long*/ , imei, imsi, msisdn, "
			 "call_date /*char*/, call_time /*char*/, duration /*int*/, msc_pointcode /*int*/, location_area /*int*/, cell /*int*/, "
			"other_party /*char*/, service_type /*short*/, service_id /*short*/, msc_id /*char*/ "
			"from BILLING.VCDR_MOBILE_ROAMER cdr where datetime>sysdate-:interval /*long*/ and status=-1 ",
              otlConnect 
             ); 

		otlStream << pastLookupInterval;
	}
	catch (otl_exception &otlEx) {
		otlConnect.rollback();
		logToDB(3, "DB error while selecting data from BILLING.VCDR_MOBILE_ROAMER:");
		logToDB(3, (char*)otlEx.msg);
		if( strlen(otlEx.stm_text) > 0 )
			logToDB( 3, (char*) otlEx.stm_text ); // log SQL that caused the error
		if( strlen(otlEx.var_info) > 0 )
			logToDB( 3, (char*) otlEx.var_info ); // log the variable that caused the error
		logToDB(3, "---- Alcatel TAP writing service finished with errors ----");
		otlLogConnect.commit();

		log("---- Alcatel TAP writing service finished with errors, see DB log ----");
		if(ofsLog.is_open()) ofsLog.close();
		exit(2);
	}

	logToDB(0, "Using filename " + filename + " with ID " + std::to_string( static_cast<unsigned long long> (file_id)));

	string rowID;
	long status;
	short transactionType;
	long billingRecordID;
	string imei;
	string imsi;
	string msisdn;
	string chrDate;
	string chrTime;
	int duration;
	int mscID;
	int locationArea;
	int cellID;
	string otherParty;
	short serviceType;
	short serviceCode;
	string mscAddress;
	int record_count = 0;

	unsigned char asnBuffer[65536];	// maybe SMALLER SIZE ??

	string fullFileName;
#ifdef WIN32
	fullFileName = "outgoing\\" + filename;
#else
	fullFileName = "outgoing/" + filename;
#endif
	
	FILE* f=NULL;
	unsigned long fileSize=0;
	int bytesWritten;
	int asnBufLen;
	bool cdrFound = false;
	bool bWarnings = false;

	while ( !otlStream.eof ()) {
		cdrFound = true;
		if(!f) {
			f = fopen (fullFileName.c_str(), "wb");
			if (!f) {
				otlConnect.rollback();
				logToDB(2, "Unable to create output file " + fullFileName + ". Exiting.");
				logToDB(2, "---- Alcatel TAP writing service finished with errors ----");
				otlLogConnect.commit();
				otlConnect.logoff();
				otlLogConnect.logoff();
				log("---- Alcatel TAP writing service finished with errors, see DB log----");
				if(ofsLog.is_open()) ofsLog.close();
				exit(1);
			}
		}
		try {
			otlStream >> rowID >> status >> transactionType >> billingRecordID >> imei >> imsi >> msisdn >> chrDate >> chrTime >> duration >> mscID >> locationArea >> cellID >> otherParty >>serviceType >> serviceCode >> mscAddress;
			CallRecord callRecord( 1 /*_callRecordType*/, billingRecordID, transactionType, imei, imsi, msisdn, chrDate, chrTime, duration, mscID, locationArea, cellID, otherParty, serviceType, serviceCode, mscAddress);
			ASNType t = callRecord.getASNObject();
			asnBufLen=t.encode(asnBuffer);

			// write until file limit is not reached or whole data is fetched
			bytesWritten = fwrite (asnBuffer, 1, asnBufLen, f);
			if(bytesWritten < asnBufLen) {
				otlConnect.rollback();
				logToDB(2, "Failed to write data to output file. Exiting.");
				logToDB(2, "---- Alcatel TAP writing service finished with errors ----");
				otlLogConnect.commit();
				otlConnect.logoff();
				otlLogConnect.logoff();
				log("---- Alcatel TAP writing service finished with errors, see DB log----");
				if(ofsLog.is_open()) ofsLog.close();
				exit(1);
			}
			fileSize += bytesWritten;

// CHECK MAX SIZE
			
			// update CDR_MOBILE
			otlUpdate.open(1, "update BILLING.CDR_MOBILE set status=:st_new/*long*/ where status=:st_old/*long*/ and rowid=chartorowid(:rw/*char[255]*/)", otlConnect);
			otlUpdate << file_id << status << rowID;
			if( otlUpdate.get_rpc() != 1) {
				otlConnect.rollback();
				logToDB(3, "Critical error: Update BILLING.CDR_MOBILE record with rowid " + rowID + " processed " + to_string( static_cast<unsigned long long> (otlUpdate.get_rpc())) +" rows. Must be 1 row only. Exiting.");
				logToDB(3, "---- Alcatel TAP writing service finished with errors ----");
				otlLogConnect.commit();
				otlConnect.logoff();
				otlLogConnect.logoff();
				log("---- Alcatel TAP writing service finished with errors, see DB log----");
				if(ofsLog.is_open()) ofsLog.close();
				exit(2);
			}

			otlUpdate.flush();
			otlUpdate.close();
			
			record_count++;
			if( fileSize >= maxAllowedFileSize) 
				break;
		}
		catch (otl_exception &otlEx) {
			otlConnect.rollback();
			otlConnect.logoff();
			logToDB(3, "DB error while processing CDR records:");
			logToDB(3, (char*)otlEx.msg);
			if( strlen(otlEx.stm_text) > 0 )
				logToDB( 1, (char*) otlEx.stm_text ); // log SQL that caused the error
			if( strlen(otlEx.var_info) > 0 )
				logToDB( 3, (char*) otlEx.var_info ); // log the variable that caused the error
			logToDB(3, "---- Alcatel TAP writing service finished with errors ----");
			otlLogConnect.commit();
			otlLogConnect.logoff();
			log("---- Alcatel TAP writing service finished with errors, see DB log ----");
			if(ofsLog.is_open()) ofsLog.close();
			exit(2);
			
		}
	}

	if(!cdrFound) {
		otlConnect.rollback();
		otlConnect.logoff();
		logToDB(0, "No roamer CDR to create TAP file found. Exiting.");
		logToDB(0, "---- Alcatel TAP writing service finished with successfully ----");
		otlLogConnect.commit();
		otlLogConnect.logoff();
		log("---- Alcatel TAP writing service finished successfully ----");
		if(ofsLog.is_open()) ofsLog.close();
		exit(0);
	}

	logToDB(0, "Successfully created CDR file with " + to_string( (unsigned long long) record_count) + " records.");

	try {
		otlUpdate.open(1  /*stream buffer size in logical rows*/, "update BILLING.TAP_FILE set sent=sysdate, record_count=:rec_cnt/*int*/ where object_no=:id/*long*/", otlConnect);
		otlUpdate << record_count << file_id;
		otlUpdate.flush();
	}
	catch (otl_exception &otlEx) {
		logToDB(1, "Unable to update BILLING.TAP_FILE table with record count " + to_string( (unsigned long long) otlUpdate.get_rpc()) + ":");
		logToDB(1, (char*)otlEx.msg);
		if( strlen(otlEx.stm_text) > 0 )
			logToDB( 1, (char*) otlEx.stm_text ); // log SQL that caused the error
		if( strlen(otlEx.var_info) > 0 )
			logToDB( 1, (char*) otlEx.var_info ); // log the variable that caused the error
	}

	otlStream.close();
	//otlConnect.commit();
	fclose(f);
	
	// Upload file to FTP-server
	if( ftpServer.size() > 0 )
		try {
			int ncftp_argc = 9;
			if (ftpPort.empty())
				ftpPort = "21";	// use default ftp port
			const char* pszArguments[] = { "ncftpput", "-u", ftpUsername.c_str(), "-p", ftpPassword.c_str(), "-P", ftpPort.c_str(), 
											ftpServer.c_str(), ftpDirectory.c_str(), fullFileName.c_str(), NULL };
			char szFtpResult[4096];
			int ftpResult = ncftp_main(ncftp_argc, (char**)pszArguments, szFtpResult);
			if( ftpResult != 0 ) {
				logToDB(2, "Error while uploading file " + filename + " on FTP server " + ftpServer + ":");
				logToDB(2, szFtpResult);
				logToDB(2, "---- Alcatel TAP writing service finished with errors ----");
				otlLogConnect.commit();
				otlLogConnect.logoff();
				otlConnect.rollback();
				otlConnect.logoff();
				log("---- Alcatel TAP writing service finished with errors, see DB log----");
				if(ofsLog.is_open()) ofsLog.close();
				exit(1);
			}
			logToDB(0, "File " + filename + " successfully uploaded to FTP server " + ftpServer);

			// move uploaded file to archive
			string archFileName;
			#ifdef WIN32
				archFileName = "archive\\" + filename;
			#else
				archFileName = "archive/" + filename;
			#endif
				
			f = fopen (fullFileName.c_str(), "rb");
			FILE *fArch = fopen( archFileName.c_str(), "wb");
			if( f && fArch ) {
				// both files opened successfully
				char buffer[65536];
				memset(buffer, 0, 65536);
				int size = fread(buffer, 1, 65536, f);
				while(size > 0)
				{
    				bytesWritten = fwrite(buffer, 1, size, fArch);
					if( bytesWritten != size ) {
						logToDB(1, "Unable to copy CDR file to archive ( " + archFileName + " ).");
						bWarnings = true;
						break;
					}
					size = fread(buffer, 1, 65536, f);
				}
				fclose(f);
				fclose(fArch);
				if ( remove(fullFileName.c_str()) != 0 ) {
					logToDB(1, "Unable to remove CDR file from outgoing folder");
					bWarnings = true;
				}
			}
			else {
				logToDB(1, "Unable to move CDR file to archive ( " + archFileName + ").");
				bWarnings = true;
			}
		}
		catch(...) {
			logToDB(2, "Exception while uploading file " + filename + " to FTP server. Uploading failed.");
			logToDB(2, "---- Alcatel TAP writing service finished with errors ----");
			otlLogConnect.commit();
			otlLogConnect.logoff();
			otlConnect.rollback();
			otlConnect.logoff();
			log("---- Alcatel TAP writing service finished with errors, see DB log ----");
			if(ofsLog.is_open()) ofsLog.close();
			exit(1);
		}
	
	otlConnect.commit();
	otlConnect.logoff();
	if( !bWarnings) 
		logToDB(0, "---- Alcatel TAP writing service finished successfully ----");
	else
		logToDB(0, "---- Alcatel TAP writing service finished with warnings ----");
	otlLogConnect.commit();
	otlLogConnect.logoff();
	if( !bWarnings) 
		log("---- Alcatel TAP writing service finished successfully ----");
	else
		log("---- Alcatel TAP writing service with warnings, see DB log ----");
	if(ofsLog.is_open()) ofsLog.close();

	return 0;
}


