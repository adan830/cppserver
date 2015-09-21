#include "network.h"
#include "besim.h"
#include "md5.h"


string rootDir = "/mnt";
string cookie_header = "Cookie: cpp_session_id=";
int cookie_header_len = 23;

extern char files[50][BUF_LEN];
extern char senddata[MAX_CPU][BUF_LEN];
extern char readbuf[MAX_CPU][BUF_LEN];
extern map<string, char *> fileMap;

map<string, string> mySession;
int sessionID = 0;


void process_besim(int fd, int cpu_id)
{
	map<string, string> reqArgs;
	string str(readbuf[cpu_id]);
	int commandType = parse_client_request(str, reqArgs);
	execute_script(fd, cpu_id, commandType, reqArgs);
}

int parse_client_request(string &str, map<string, string> &reqArgs)
{
	int commandType;

	if (str.find(".php") == string::npos)
	{
		if (str.find("If-Modified-Since:") != string::npos)
			commandType = FILE_304;
		else
			commandType = FILE_200;
	}
	else
	{
		if (str.find("account_summary.php") != string::npos)
			commandType = ACCOUNT_SUMMARY;
		else if (str.find("add_payee.php") != string::npos)
			commandType = ADD_PAYEE;
		else if (str.find("bill_pay.php") != string::npos)
			commandType = BILL_PAY;
		else if (str.find("bill_pay_status_output.php") != string::npos)
			commandType = BILL_PAY_STATUS_OUTPUT;
		else if (str.find("check_detail_html.php") != string::npos)
			commandType = CHECK_DETAIL_HTML;
		else if (str.find("check_detail_image.php") != string::npos)
			commandType = CHECK_DETAIL_IMAGE;
		else if (str.find("init.php") != string::npos)
			commandType = INIT;
		else if (str.find("logout.php") != string::npos)
			commandType = LOGOUT;
		else if (str.find("order_check.php") != string::npos)
			commandType = ORDER_CHECK;
		else if (str.find("/profile.php") != string::npos)
			commandType = PROFILE;
		else if (str.find("/transfer.php") != string::npos)
			commandType = TRANSFER;
		else if (str.find("change_profile.php") != string::npos)
			commandType = CHANGE_PROFILE;
		else if (str.find("login.php") != string::npos)
			commandType = LOGIN;
		else if (str.find("place_check_order.php") != string::npos)
			commandType = PLACE_CHECK_ORDER;
		else if (str.find("post_payee.php") != string::npos)
			commandType = POST_PAYEE;
		else if (str.find("post_transfer.php") != string::npos)
			commandType = POST_TRANSFER;
		else if (str.find("quick_pay.php") != string::npos)
			commandType = QUICK_PAY;
		else
		{
			perror("error client request");
			exit(-1);
		}
	}

	getArgs(commandType, str, reqArgs);

	/*map<string,string>::iterator it;
	for(it=reqArgs.begin();it!=reqArgs.end();++it)
		cout <<"key: "<<it->first <<"\tvalue: "<<it->second<<endl;*/

	return commandType;
}

void getArgs(int commandType, string &str, map<string, string> &reqArgs)
{
	size_t startPos, endPos;
	int len;
	string tmpArg, tmpKey, tmpValue;

	size_t cookie_lpos = str.find(cookie_header), cookie_rpos;
	if (cookie_lpos != string::npos)
	{
		cookie_lpos += cookie_header_len;
		cookie_rpos = str.find("\r\n", cookie_lpos);
		reqArgs["sessionID"] = str.substr(cookie_lpos, cookie_rpos - cookie_lpos);
	}

	switch(commandType)
	{
	case FILE_200:
		{
			startPos = str.find('/');
			endPos = str.find(" HTTP/1.1");
			len = endPos - startPos;
			tmpArg = str.substr(startPos, len);
			reqArgs["filePath"] = tmpArg;

			break;
		}

	case FILE_304:
		{
			startPos = str.find('/');
			endPos = str.find(" HTTP/1.1");
			len = endPos - startPos;
			tmpArg = str.substr(startPos, len);
			reqArgs["filePath"] = tmpArg;

			startPos = str.find("If-Modified-Since:")+19;
			tmpArg = str.substr(startPos);
			reqArgs["time"] = tmpArg;

			break;
		}

	case ACCOUNT_SUMMARY: break;
	case ADD_PAYEE:
	case BILL_PAY:
	case BILL_PAY_STATUS_OUTPUT:
	case CHECK_DETAIL_HTML:
	case CHECK_DETAIL_IMAGE:
	case INIT:
	case LOGOUT:
	case ORDER_CHECK:
	case PROFILE:
	case TRANSFER:
		{
			startPos = str.find('?') + 1;
			endPos = str.find(" HTTP/1.1");
			size_t lpos = startPos, rpos = startPos;
			for (size_t i = startPos; i < endPos; i++)
			{
				if (i < endPos-1)
				{
					if (str[i] == '=')
					{
						rpos = i;
						tmpKey = str.substr(lpos, rpos-lpos);
						lpos = i+1;
					}
					else if (str[i] == '&')
					{
						rpos = i;
						tmpValue = str.substr(lpos, rpos-lpos);
						lpos = i+1;
						reqArgs[tmpKey] = tmpValue;
					}
				}
				else
				{
					tmpValue = str.substr(lpos, i-lpos+1);
					reqArgs[tmpKey] = tmpValue;
				}
			}
			break;
		}

	default:
		{
			startPos = str.find("\r\n\r\n") + 4;
			endPos = str.length()-1;
			size_t lpos = startPos, rpos = startPos;
			for (size_t i = startPos; i < endPos; i++)
			{
				if (i < endPos-1)
				{
					if (str[i] == '=')
					{
						rpos = i;
						tmpKey = str.substr(lpos, rpos-lpos);
						lpos = i+1;
					}
					else if (str[i] == '&')
					{
						rpos = i;
						tmpValue = str.substr(lpos, rpos-lpos);
						lpos = i+1;
						reqArgs[tmpKey] = tmpValue;
					}
				}
				else
				{
					tmpValue = str.substr(lpos, i-lpos+1);
					reqArgs[tmpKey] = tmpValue;
				}
			}
			break;
		}
	}
}

void backend_get(int fd, string &request, struct besimReturn& returnData)
{
	char buf[BUF_LEN];
	//cout << request << endl;
	//read write need change if fd is no-blocking socket
	int rtn, left = request.length(), sended = 0, received = 0;
    memcpy(buf, request.c_str(), left);
    buf[left] = 0;
    while (true)
    {
        rtn = write(fd, buf+sended, left);
        if (rtn < 0)
        {
            perror("write besim error");
            exit(-1);
        }
        sended += rtn;
        left -= rtn;
        if (left > 0)
            continue;
        break;
    }
//cout << sended << endl;
/*	rtn = read(fd, buf, BUF_LEN);
	if (rtn < 0)
	{
		perror("read besim error");
		exit(-1);
	}
	buf[rtn] = 0; */
    while (true)
    {
        rtn = read(fd, buf+received, BUF_LEN);
        if (rtn < 0)
        {
            perror("read besim error");
            exit(-1);
        }
        received += rtn;
        buf[received] = 0;
        string tmp = buf;
        size_t tmp_spos = tmp.find("Content-Length:"), tmp_epos = tmp.find("\r\n\r\n");
        if (tmp_spos == string::npos || tmp_epos == string::npos)
            continue;
        tmp_spos += 16;
        string tmp_len = tmp.substr(tmp_spos, tmp_epos-tmp_spos);
        size_t real_len = tmp.length() - (tmp_epos+4);
        stringstream os;
        size_t expect_len = (size_t)(atoi(tmp_len.c_str()));
        if (real_len < expect_len)
            continue;
        //cout << real_len << endl;
        break;
    }
	//cout<<buf<<endl;

	string tmpstr = buf;
	size_t startPos = tmpstr.find("<pre>\n"), endPos = tmpstr.find("\n</pre>");
	if (startPos == string::npos || endPos == string::npos)
	{
		perror("find string <pre>, </pre> error");
		exit(-1);
	}
	startPos += 6;
	returnData.responseCode = tmpstr[startPos] - '0';
	startPos += 2;
	size_t tmpStart = startPos;
	size_t tmpEnd = tmpstr.find('\n', tmpStart);
	int index = 0;
	while (tmpEnd <= endPos)
	{
		for (size_t tmpPos = tmpStart, i = tmpStart; i < tmpEnd; i++)
		{
			if (i < tmpEnd-1)
			{
				if (tmpstr[i] == '&')
				{
					returnData.rtnArgs[index].push_back(tmpstr.substr(tmpPos, i-tmpPos));
					tmpPos = i+1;
				}
			}
			else
				returnData.rtnArgs[index].push_back(tmpstr.substr(tmpPos, i-tmpPos+1));
		}
		index++;
		tmpStart = tmpEnd+1;
		tmpEnd = tmpstr.find('\n', tmpStart);
		if (tmpEnd == string::npos)
			break;
	}
	returnData.argArrays = index;
/*	printf("\n------------\nbesim responseCode:\t%d\n", returnData.responseCode);
	printf("besim return args:\n");
	for (int j = 0; j < returnData.argArrays; j++)
	{
		for (int i = 0; i < returnData.rtnArgs[j].size(); i++)
			cout << i+1 << '\t' <<returnData.rtnArgs[j][i] << endl;
		cout << "*********" << endl;
	}
	printf("\n---------------------\n"); 
*/
}

void init(int cpu_id)
{
	time_t tnow = time(NULL);
	stringstream os;
	os << "<html><head><title>SPECweb2009 Banking Workload Init</title>"
	   << "</head><body><p>SERVER_SOFTWARE = c1000k_server</p>"
	   << "<p>REMOTE_ADDR = 10.61.0.118</p><p>SCRIPT_NAME = init.php</p>"
	   << "<p>QUERY_STRING = ...</p><p>SERVER_TIME = " << tnow << "000</p></body></html>";
	string tmpContent = os.str();
	os.str("");
	os << "HTTP/1.1 200 OK\r\nContent-Length: " << tmpContent.length()
	   << "\r\nConnection: keep-alive\r\n\r\n" << tmpContent;
	tmpContent = os.str();
	size_t len = tmpContent.length();
	memcpy(senddata[cpu_id], tmpContent.c_str(), len);
	senddata[cpu_id][len] = 0;
	//printf("Response init:\n%s\n", senddata[cpu_id]);
}

void file_200(int cpu_id, map<string, string> &reqArgs)
{
	size_t len;
	string returnHead, filePath = rootDir + reqArgs["filePath"];
	fstream infile;
	infile.open(filePath.c_str(), ios::in);
	if (!infile.is_open())
	{
		perror("open file error");
		returnHead = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
		len = returnHead.length();
		memcpy(senddata[cpu_id], returnHead.c_str(), len);
		senddata[cpu_id][len] = 0;
	}
	else
	{
		char fileBuf[BUF_LEN], buffer[50];

		infile.read(fileBuf, BUF_LEN);
		fileBuf[infile.gcount()] = 0;
		returnHead = "HTTP/1.1 200 OK\r\nLast-Modified: ";

		struct stat fileStat;
		stat(filePath.c_str(), &fileStat);
		time_t tmpTime = fileStat.st_mtime;
		struct tm *timeinfo;
		timeinfo = gmtime(&tmpTime);
		strftime(buffer, 50 ,"%a, %d %b %Y %T GMT",timeinfo);

		stringstream os;
		os << returnHead << buffer <<"\r\nContent-Length: " << strlen(fileBuf)
		   << "\r\nConnection: keep-alive\r\n\r\n" << fileBuf;
		string tmpContent = os.str();
		len = tmpContent.length();
		memcpy(senddata[cpu_id], tmpContent.c_str(), len);
		senddata[cpu_id][len] = 0;

		infile.close();
	}
}

void file_304(int cpu_id, map<string, string> &reqArgs)
{
	size_t len;
	string returnHead, filePath = rootDir + reqArgs["filePath"];
	fstream infile;
	infile.open(filePath.c_str(), ios::in);
	if (!infile.is_open())
	{
		perror("open file error");
		returnHead = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
		len = returnHead.length();
		memcpy(senddata[cpu_id], returnHead.c_str(), len);
		senddata[cpu_id][len] = 0;
	}
	else
	{
		struct stat fileStat;
		stat(filePath.c_str(), &fileStat);
		time_t tmpTime = fileStat.st_mtime;
		struct tm *timeinfo;
		char buffer [50];
		timeinfo = gmtime (&tmpTime);
		strftime (buffer, 50 ,"%a, %d %b %Y %T GMT",timeinfo);
		if (strcmp(buffer, reqArgs["time"].c_str()) == 0)
		{
			returnHead = "HTTP/1.1 304 Not Modified";
			len = returnHead.length();
			memcpy(senddata[cpu_id], returnHead.c_str(), len);
			senddata[cpu_id][len] = 0;
		}
		else
		{
			char fileBuf[BUF_LEN];
			infile.read(fileBuf, BUF_LEN);
			fileBuf[infile.gcount()] = 0;
			returnHead = "HTTP/1.1 200 OK\r\nLast-Modified: ";

			stringstream os;
			os << returnHead << buffer <<"\r\nContent-Length: " << strlen(fileBuf)
			   << "\r\nConnection: keep-alive\r\n\r\n" << fileBuf;
			string tmpContent = os.str();
			len = tmpContent.length();
			memcpy(senddata[cpu_id], tmpContent.c_str(), len);
			senddata[cpu_id][len] = 0;
		}
		infile.close();
	}
}

void login(int fd, int cpu_id, map<string, string> &reqArgs)
{
	/*map<string,string>::iterator it;
	for(it=reqArgs.begin();it!=reqArgs.end();++it)
		cout <<"key: "<<it->first <<"\tvalue: "<<it->second<<endl;*/
//cout << "----------" << reqArgs.size() << endl;
	stringstream os;
	string request = "1&1&", tmpContent;
	request += reqArgs["userid"];
    /*os.str("");
    os << "Content-Length: " << request.length() << "\r\n\r\n" << request;
    request = os.str();*/
    //cout << request <<endl;
    os.str("");
    os << "GET /api_dir/besim_api?" << request << " HTTP/1.1";
    request = os.str();
	struct besimReturn returnData_1, returnData_2;
	char fileBuf[BUF_LEN];

	backend_get(fd, request, returnData_1);
	if (returnData_1.responseCode != OK_STATUS)
    {
    	perror("besim login return error");
    	exit(-1);
    }

    char *file_0 = NULL, *file_1 = NULL, *file_2 = NULL;
	MD5 tmp_md5(reqArgs["password"]);
	string real_pass = tmp_md5.md5();
      //  cout<<real_pass<<endl;
      //  cout<<returnData_1.rtnArgs[0][0]<<endl;
	if (real_pass == returnData_1.rtnArgs[0][0])
	{
		request = "1&2&";
		request += reqArgs["userid"];
        //cout << request <<endl;
        /*os.str("");
        os << "Content-Length: " << request.length() << "\r\n\r\n" << request;
        request = os.str();*/

        os.str("");
        os << "GET /api_dir/besim_api?" << request << " HTTP/1.1";
        request = os.str();
		backend_get(fd, request, returnData_2);
		if (returnData_2.responseCode != OK_STATUS)
		{
			perror("besim account_balance return error");
			exit(-1);
		}

		file_0 = fileMap["welcome.tpl"];

        os.str("");
		for (int i = 0; i < returnData_2.argArrays; i++)
		{
	//		cout<<returnData_2.rtnArgs[i][1]<<endl;
			os << "<tr><td>" << returnData_2.rtnArgs[i][0] << "</td><td>";
			if (returnData_2.rtnArgs[i][1] == "01")
				os << "Checking";
			else if (returnData_2.rtnArgs[i][1] == "02")
				os << "Saving";
			else
				os << "Other";
			os << "</td><td>" << returnData_2.rtnArgs[i][2] << "</td></tr>";
		}
		string tmpstr = os.str();

		file_1 = fileMap["menu.tpl"];
		file_2 = fileMap["welcome"];

		snprintf(fileBuf, BUF_LEN, file_0, (reqArgs["userid"]).c_str(),
			tmpstr.c_str(), file_1, file_2);

		os.str("");
		os << cpu_id << sessionID;
		mySession[os.str()] = reqArgs["userid"];

		os.str("");
		os << "HTTP/1.1 200 OK\r\nSet-Cookie: cpp_session_id="
			<< cpu_id << sessionID << "\r\nContent-Length: "<< strlen(fileBuf)
			<< "\r\nConnection: keep-alive\r\n\r\n" << fileBuf;
		sessionID++;
	}
	else
	{
		file_0 = fileMap["login.tpl"];
		snprintf(fileBuf, BUF_LEN, file_0, "Incorrect user id or password!");
		os << "HTTP/1.1 200 OK\r\nContent-Length: " << strlen(fileBuf)
		<< "\r\nConnection: keep-alive\r\n\r\n" << fileBuf;
	}

	tmpContent = os.str();
	size_t len = tmpContent.length();
	memcpy(senddata[cpu_id], tmpContent.c_str(), len);
	senddata[cpu_id][len] = 0;
}

void account_summary(int fd, int cpu_id, map<string, string> &reqArgs)
{
	stringstream os;
	string request = "1&3&", tmpContent;
	char fileBuf[BUF_LEN];
	char *file_0 = NULL, *file_1 = NULL, *file_2 = NULL;

	map<string,string>::iterator iter = mySession.find(reqArgs["sessionID"]);
	if(iter == mySession.end())
	{
		file_0 = fileMap["login.tpl"];
		snprintf(fileBuf, BUF_LEN, file_0, "Please login!");
	}
	else
	{
		struct besimReturn returnData;
		request += iter->second;
        os.str("");
        os << "GET /api_dir/besim_api?" << request << " HTTP/1.1";
        request = os.str();
		backend_get(fd, request, returnData);
		if (returnData.responseCode != OK_STATUS)
		{
	    	/*perror("besim account_summary return error code");
	    	exit(-1);*/
	    	file_0 = fileMap["message.tpl"];
	    	snprintf(fileBuf, BUF_LEN, file_0, "besim account_summary return error code: 1");
	    }
	    else
	    {
	    	file_0 = fileMap["account_summary.tpl"];

            os.str("");
			for (int i = 0; i < returnData.argArrays; i++)
			{
				os << "<tr><td>" << returnData.rtnArgs[i][0] << "</td><td>";
				if (returnData.rtnArgs[i][1] == "01")
					os << "Checking";
				else if (returnData.rtnArgs[i][1] == "02")
					os << "Saving";
				else
					os << "Other";
				os << "</td><td>" << returnData.rtnArgs[i][2] << "</td><td>"
					<< returnData.rtnArgs[i][3] << "</td><td>"
					<< returnData.rtnArgs[i][4] << "</td><td>"
					<< returnData.rtnArgs[i][5] << "</td><td>"
					<< returnData.rtnArgs[i][6] << "</td></tr>";
			}
			string tmpstr = os.str();

			file_1 = fileMap["menu.tpl"];
			file_2 = fileMap["account_summary"];

			snprintf(fileBuf, BUF_LEN, file_0, (iter->second).c_str(),
				tmpstr.c_str(), file_1, file_2);
	    }
	}

	os.str("");
	os << "HTTP/1.1 200 OK\r\nContent-Length: " << strlen(fileBuf)
		<< "\r\nConnection: keep-alive\r\n\r\n" << fileBuf;
	tmpContent = os.str();
	size_t len = tmpContent.length();
	memcpy(senddata[cpu_id], tmpContent.c_str(), len);
	senddata[cpu_id][len] = 0;
}

void check_detail_html(int fd, int cpu_id, map<string, string> &reqArgs)
{
	stringstream os;
	string request = "1&4&", tmpContent;
	char fileBuf[BUF_LEN];
	char *file_0 = NULL, *file_1 = NULL, *file_2 = NULL;

	map<string,string>::iterator iter = mySession.find(reqArgs["sessionID"]);
	if(iter == mySession.end())
	{
		file_0 = fileMap["login.tpl"];
		snprintf(fileBuf, BUF_LEN, file_0, "Please login!");
	}
	else
	{
		struct besimReturn returnData;
		request += iter->second;
		request += '&';
		request += reqArgs["check_no"];
        os.str("");
        os << "GET /api_dir/besim_api?" << request << " HTTP/1.1";
        request = os.str();
		backend_get(fd, request, returnData);
		if (returnData.responseCode != OK_STATUS)
		{
	    	/*perror("besim check_detail_html return error code");
	    	exit(-1);*/
	    	file_0 = fileMap["message.tpl"];
	    	snprintf(fileBuf, BUF_LEN, file_0, "besim check_detail_html return error code: 1");
	    }
	    else
	    {
	    	string tmpstr = returnData.rtnArgs[0][0];
	    	file_0 = fileMap["check_detail_html.tpl"];
			file_1 = fileMap["menu.tpl"];
			file_2 = fileMap["check_detail_html"];
			snprintf(fileBuf, BUF_LEN, file_0, (iter->second).c_str(),
				tmpstr.c_str(), (reqArgs["check_no"]).c_str(), file_1, file_2);
	    }
	}

	os.str("");
	os << "HTTP/1.1 200 OK\r\nContent-Length: " << strlen(fileBuf)
		<< "\r\nConnection: keep-alive\r\n\r\n" << fileBuf;
	tmpContent = os.str();
	size_t len = tmpContent.length();
	memcpy(senddata[cpu_id], tmpContent.c_str(), len);
	senddata[cpu_id][len] = 0;
}

void check_detail_image(int fd, int cpu_id, map<string, string> &reqArgs)
{
	stringstream os;
	string request = "1&4&", tmpContent;
	char fileBuf[BUF_LEN];
	char *file_0 = NULL;

	map<string,string>::iterator iter = mySession.find(reqArgs["sessionID"]);
	if(iter == mySession.end())
	{
		char *file_0 = fileMap["login.tpl"];
		snprintf(fileBuf, BUF_LEN, file_0, "Please login!");
	}
	else
	{
		struct besimReturn returnData;
		request += iter->second;
		request += '&';
		request += reqArgs["check_no"];
        os.str("");
        os << "GET /api_dir/besim_api?" << request << " HTTP/1.1";
        request = os.str();
		backend_get(fd, request, returnData);
		if (returnData.responseCode != OK_STATUS)
		{
	    	/*perror("besim check_detail_image return error code");
	    	exit(-1);*/
	    	file_0 = fileMap["message.tpl"];
	    	snprintf(fileBuf, BUF_LEN, file_0, "besim check_detail_image return error code: 1");
	    }
	    else
	    {
	    	string imagePath;
		    if (reqArgs["side"] == "front")
		    	imagePath = returnData.rtnArgs[0][1];
		    else
		    	imagePath = returnData.rtnArgs[0][2];

		    fstream infile(imagePath.c_str(), ios::in);
		    if (!infile.is_open())
		    {
		    	perror("open image file error");
		    	exit(-1);
		    }
		    infile.read(fileBuf, BUF_LEN);
		    fileBuf[infile.gcount()] = 0;
		    infile.close();
	    }
	}

	os.str("");
	os << "HTTP/1.1 200 OK\r\nContent-Length: " << strlen(fileBuf)
		<< "\r\nConnection: keep-alive\r\n\r\n" << fileBuf;
	tmpContent = os.str();
	size_t len = tmpContent.length();
	memcpy(senddata[cpu_id], tmpContent.c_str(), len);
	senddata[cpu_id][len] = 0;
}

void bill_pay(int fd, int cpu_id, map<string, string> &reqArgs)
{
	stringstream os;
	string request = "1&5&", tmpContent;
	char fileBuf[BUF_LEN];
	char *file_0 = NULL, *file_1 = NULL, *file_2 = NULL;

	map<string,string>::iterator iter = mySession.find(reqArgs["sessionID"]);
	if(iter == mySession.end())
	{
		file_0 = fileMap["login.tpl"];
		snprintf(fileBuf, BUF_LEN, file_0, "Please login!");
	}
	else
	{
		struct besimReturn returnData;
		request += iter->second;
        os.str("");
        os << "GET /api_dir/besim_api?" << request << " HTTP/1.1";
        request = os.str();
		backend_get(fd, request, returnData);
		if (returnData.responseCode != OK_STATUS)
		{
	    	perror("besim bill_pay return error code: 1");
	    	exit(-1);
	    }

	    file_0 = fileMap["bill_pay.tpl"];

        os.str("");
		for (int i = 0; i < returnData.argArrays; i++)
		{
			os << "<tr><td>" << returnData.rtnArgs[i][0] << "</td><td>"
				<< returnData.rtnArgs[i][2] << "</td><td>"
				<< returnData.rtnArgs[i][1] << "</td></tr>";
		}
		string tmpstr = os.str();

		file_1 = fileMap["menu.tpl"];
		file_2 = fileMap["bill_pay"];

		snprintf(fileBuf, BUF_LEN, file_0, (iter->second).c_str(),
			tmpstr.c_str(), file_1, file_2);
	}

	os.str("");
	os << "HTTP/1.1 200 OK\r\nContent-Length: " << strlen(fileBuf)
		<< "\r\nConnection: keep-alive\r\n\r\n" << fileBuf;
	tmpContent = os.str();
	size_t len = tmpContent.length();
	memcpy(senddata[cpu_id], tmpContent.c_str(), len);
	senddata[cpu_id][len] = 0;
}

void add_payee(int cpu_id, map<string, string> &reqArgs)
{
	stringstream os;
	string tmpContent;
	char fileBuf[BUF_LEN];
	char *file_0 = NULL, *file_1 = NULL, *file_2 = NULL;

	map<string,string>::iterator iter = mySession.find(reqArgs["sessionID"]);
	if(iter == mySession.end())
	{
		file_0 = fileMap["login.tpl"];
		snprintf(fileBuf, BUF_LEN, file_0, "Please login!");
	}
	else
	{
	    file_0 = fileMap["add_payee.tpl"];
		file_1 = fileMap["menu.tpl"];
		file_2 = fileMap["add_payee"];
		snprintf(fileBuf, BUF_LEN, file_0, (iter->second).c_str(), file_1, file_2);
	}

	os << "HTTP/1.1 200 OK\r\nContent-Length: " << strlen(fileBuf)
		<< "\r\nConnection: keep-alive\r\n\r\n" << fileBuf;
	tmpContent = os.str();
	size_t len = tmpContent.length();
	memcpy(senddata[cpu_id], tmpContent.c_str(), len);
	senddata[cpu_id][len] = 0;
}

void post_payee(int fd, int cpu_id, map<string, string> &reqArgs)
{
	stringstream os;
	string tmpContent;
	char fileBuf[BUF_LEN];
	char *file_0 = NULL, *file_1 = NULL, *file_2 = NULL;

	map<string,string>::iterator iter = mySession.find(reqArgs["sessionID"]);
	if(iter == mySession.end())
	{
		file_0 = fileMap["login.tpl"];
		snprintf(fileBuf, BUF_LEN, file_0, "Please login!");
	}
	else
	{
		struct besimReturn returnData;
		os << "GET /api_dir/besim_api?1&6&" << iter->second << '&' << reqArgs["payee_id"] << '&'
			<< reqArgs["name"] << '&' << reqArgs["street"] << '&' << reqArgs["city"] << '&'
			<< reqArgs["state"] << '&' << reqArgs["zip"] << '&' << reqArgs["phone"] << " HTTP/1.1";
		string request = os.str();
		backend_get(fd, request, returnData);
		if (returnData.responseCode != OK_STATUS)
		{
	    	/*perror("besim post_payee return error code");
	    	exit(-1);*/
	    	file_0 = fileMap["message.tpl"];
	    	snprintf(fileBuf, BUF_LEN, file_0, "besim post_payee return error code: 1");
	    }
	    else
	    {
	    	string tmpstr = returnData.rtnArgs[0][0], msg = "Payee ";
		    msg += reqArgs["payee_id"];
		    msg += " succesfully added!";
		    file_0 = fileMap["post_payee.tpl"];
			file_1 = fileMap["menu.tpl"];
			file_2 = fileMap["post_payee"];
			snprintf(fileBuf, BUF_LEN, file_0, (iter->second).c_str(),
				msg.c_str(), tmpstr.c_str(), file_1, file_2);

	    }
	}

	os.str("");
	os << "HTTP/1.1 200 OK\r\nContent-Length: " << strlen(fileBuf)
		<< "\r\nConnection: keep-alive\r\n\r\n" << fileBuf;
	tmpContent = os.str();
	size_t len = tmpContent.length();
	memcpy(senddata[cpu_id], tmpContent.c_str(), len);
	senddata[cpu_id][len] = 0;
}

void quick_pay(int fd, int cpu_id, map<string, string> &reqArgs)
{
	stringstream os;
	string tmpContent;
	char fileBuf[BUF_LEN];
	char *file_0 = NULL, *file_1 = NULL, *file_2 = NULL;

	map<string,string>::iterator iter = mySession.find(reqArgs["sessionID"]);
	if(iter == mySession.end())
	{
		file_0 = fileMap["login.tpl"];
		snprintf(fileBuf, BUF_LEN, file_0, "Please login!");
	}
	else
	{
		string msg , conf, payee = "payee[0]", date = "date[0]", amount = "amount[0]";
		for (size_t i = 0; i < (reqArgs.size()-1)/3; i++)
		{
			struct besimReturn returnData;
			os.str("");
			os << "GET /api_dir/besim_api?1&7&" << iter->second << '&' << reqArgs[payee] << '&'
				<< reqArgs[date] << '&' << reqArgs[amount] << " HTTP/1.1";
			string request = os.str();
			backend_get(fd, request, returnData);
			if (returnData.responseCode != OK_STATUS)
			{
		    	/*perror("besim quick_pay return error code");
		    	exit(-1);*/
		    	os.str("");
			    os << "<tr><td>Failed: payee=" << reqArgs[payee] <<
			    	", besim quick_pay return error code: 1</td></tr>";
			    msg += os.str();
		    }
		    else
		    {
		    	os.str("");
			    os << "<tr><td>Scheduled: payee=" << reqArgs[payee] << ", date="
			    	<< reqArgs[date] << ", amount=" << reqArgs[amount] << "</td></tr>";
			    msg += os.str();
			    conf = returnData.rtnArgs[0][0];
		    }

		    payee[6] += 1;
			date[5] += 1;
			amount[7] += 1;
		}

		file_0 = fileMap["quick_pay.tpl"];
		file_1 = fileMap["menu.tpl"];
		file_2 = fileMap["quick_pay"];

		snprintf(fileBuf, BUF_LEN, file_0, (iter->second).c_str(),
			msg.c_str(), conf.c_str(), file_1, file_2);
	}

	os.str("");
	os << "HTTP/1.1 200 OK\r\nContent-Length: " << strlen(fileBuf)
		<< "\r\nConnection: keep-alive\r\n\r\n" << fileBuf;
	tmpContent = os.str();
	size_t len = tmpContent.length();
	memcpy(senddata[cpu_id], tmpContent.c_str(), len);
	senddata[cpu_id][len] = 0;
}

void bill_pay_status_output(int fd, int cpu_id, map<string, string> &reqArgs)
{
	stringstream os;
	string tmpContent;
	char fileBuf[BUF_LEN];
	char *file_0 = NULL, *file_1 = NULL, *file_2 = NULL;

	map<string,string>::iterator iter = mySession.find(reqArgs["sessionID"]);
	if(iter == mySession.end())
	{
		file_0 = fileMap["login.tpl"];
		snprintf(fileBuf, BUF_LEN, file_0, "Please login!");
	}
	else
	{
		struct besimReturn returnData;
		os << "GET /api_dir/besim_api?1&8&" << iter->second << '&' << reqArgs["start"] << '&' << reqArgs["end"] << " HTTP/1.1";
		string request = os.str();
		backend_get(fd, request, returnData);
		if (returnData.responseCode != OK_STATUS)
		{
	    	/*perror("besim bill_pay_status_output return error code");
	    	exit(-1);*/
	    	file_0 = fileMap["message.tpl"];
	    	snprintf(fileBuf, BUF_LEN, file_0, "besim bill_pay_status_output return error code: 1");
	    }
	    else
	    {
	    	file_0 = fileMap["bill_pay_status_output.tpl"];

		    os.str("");
			for (int i = 0; i < returnData.argArrays; i++)
			{
				os << "<tr><td>" << returnData.rtnArgs[i][0] << "</td><td>"
					<< returnData.rtnArgs[i][1] << "</td><td align='right'>"
					<< returnData.rtnArgs[i][2] << "</td><td>Pending</td></tr>";
			}
			string tmpstr = os.str();

			file_1 = fileMap["menu.tpl"];
			file_2 = fileMap["bill_pay_status_output"];

			snprintf(fileBuf, BUF_LEN, file_0, (iter->second).c_str(),
				tmpstr.c_str(), file_1, file_2);
	    }
	}

	os.str("");
	os << "HTTP/1.1 200 OK\r\nContent-Length: " << strlen(fileBuf)
		<< "\r\nConnection: keep-alive\r\n\r\n" << fileBuf;
	tmpContent = os.str();
	size_t len = tmpContent.length();
	memcpy(senddata[cpu_id], tmpContent.c_str(), len);
	senddata[cpu_id][len] = 0;
}

void profile(int fd, int cpu_id, map<string, string> &reqArgs)
{
	stringstream os;
	string request = "1&9&", tmpContent;
	char fileBuf[BUF_LEN];
	char *file_0 = NULL, *file_1 = NULL, *file_2 = NULL;

	map<string,string>::iterator iter = mySession.find(reqArgs["sessionID"]);
	if(iter == mySession.end())
	{
		file_0 = fileMap["login.tpl"];
		snprintf(fileBuf, BUF_LEN, file_0, "Please login!");
	}
	else
	{
		struct besimReturn returnData;
		//request += iter->userid;
		request += iter->second;
        os.str("");
        os << "GET /api_dir/besim_api?" << request << " HTTP/1.1";
        request = os.str();
		backend_get(fd, request, returnData);
		if (returnData.responseCode != OK_STATUS)
		{
	    	/*perror("besim profile return error code");
	    	exit(-1);*/
	    	file_0 = fileMap["message.tpl"];
	    	snprintf(fileBuf, BUF_LEN, file_0, "besim profile return error code: 1");
	    }
	    else
	    {
	    	file_0 = fileMap["profile.tpl"];
			file_1 = fileMap["menu.tpl"];
			file_2 = fileMap["profile"];
			//字符转义？
			snprintf(fileBuf, BUF_LEN, file_0, (iter->second).c_str(),
				(returnData.rtnArgs[0][0]).c_str(), (returnData.rtnArgs[0][1]).c_str(),
				(returnData.rtnArgs[0][2]).c_str(), (returnData.rtnArgs[0][3]).c_str(),
				(returnData.rtnArgs[0][5]).c_str(), (returnData.rtnArgs[0][4]).c_str(),
				file_1, file_2);
	    }
	}

	os.str("");
	os << "HTTP/1.1 200 OK\r\nContent-Length: " << strlen(fileBuf)
		<< "\r\nConnection: keep-alive\r\n\r\n" << fileBuf;
	tmpContent = os.str();
	size_t len = tmpContent.length();
	memcpy(senddata[cpu_id], tmpContent.c_str(), len);
	senddata[cpu_id][len] = 0;
}

void change_profile(int fd, int cpu_id, map<string, string> &reqArgs)
{
	stringstream os;
	string tmpContent;
	char fileBuf[BUF_LEN];
	char *file_0 = NULL, *file_1 = NULL, *file_2 = NULL;

	map<string,string>::iterator iter = mySession.find(reqArgs["sessionID"]);
	if(iter == mySession.end())
	{
		file_0 = fileMap["login.tpl"];
		snprintf(fileBuf, BUF_LEN, file_0, "Please login!");
	}
	else
	{
		struct besimReturn returnData;
		os << "GET /api_dir/besim_api?1&10&" << iter->second << '&' << reqArgs["street"] << '&' << reqArgs["city"]
			<< '&' << reqArgs["state"] << '&' << reqArgs["zip"] << '&' << reqArgs["email"]
			<< '&' << reqArgs["phone"] << " HTTP/1.1";
    string request = os.str(), msg;
		backend_get(fd, request, returnData);
		if (returnData.responseCode != OK_STATUS)
		{
	    	/*perror("besim change_profile return error code");
	    	exit(-1);*/
	    	msg = "Update profile failed!";
	    }
	    else
	    {
	    	msg = "Profile Changed.";
	    }

	    file_0 = fileMap["change_profile.tpl"];
		file_1 = fileMap["menu.tpl"];
		file_2 = fileMap["change_profile"];

		snprintf(fileBuf, BUF_LEN, file_0, (iter->second).c_str(),
			msg.c_str(), (returnData.rtnArgs[0][0]).c_str(), file_1, file_2);
	}

	os.str("");
	os << "HTTP/1.1 200 OK\r\nContent-Length: " << strlen(fileBuf)
		<< "\r\nConnection: keep-alive\r\n\r\n" << fileBuf;
	tmpContent = os.str();
	size_t len = tmpContent.length();
	memcpy(senddata[cpu_id], tmpContent.c_str(), len);
	senddata[cpu_id][len] = 0;
}

void order_check(int fd, int cpu_id, map<string, string> &reqArgs)
{
	stringstream os;
	string tmpContent;
	char fileBuf[BUF_LEN];
	char *file_0 = NULL, *file_1 = NULL, *file_2 = NULL;

	map<string,string>::iterator iter = mySession.find(reqArgs["sessionID"]);
	if(iter == mySession.end())
	{
		file_0 = fileMap["login.tpl"];
		snprintf(fileBuf, BUF_LEN, file_0, "Please login!");
	}
	else
	{
		struct besimReturn returnData;
		string request = "1&2&";
		request += iter->second;
        os.str("");
        os << "GET /api_dir/besim_api?" << request << " HTTP/1.1";
        request = os.str();
		backend_get(fd, request, returnData);
		if (returnData.responseCode != OK_STATUS)
		{
	    	perror("besim order_check return error code");
	    	exit(-1);
	    }

	    file_0 = fileMap["order_check.tpl"];

        os.str("");
	    for (int i = 0; i < returnData.argArrays; i++)
		{
		//	cout<<returnData.rtnArgs[i][1]<<endl;
			os << "<tr><td>" << returnData.rtnArgs[i][0] << "</td><td>";
			if (returnData.rtnArgs[i][1] == "01")
				os << "Checking";
			else if (returnData.rtnArgs[i][1] == "02")
				os << "Saving";
			else
				os << "Other";
			os << "</td><td>" << returnData.rtnArgs[i][2] << "</td></tr>";
		}
		string tmpstr = os.str();

		file_1 = fileMap["menu.tpl"];
		file_2 = fileMap["order_check"];

		snprintf(fileBuf, BUF_LEN, file_0, (iter->second).c_str(),
			tmpstr.c_str(), file_1, file_2);
	}

	os.str("");
	os << "HTTP/1.1 200 OK\r\nContent-Length: " << strlen(fileBuf)
		<< "\r\nConnection: keep-alive\r\n\r\n" << fileBuf;
	tmpContent = os.str();
	size_t len = tmpContent.length();
	memcpy(senddata[cpu_id], tmpContent.c_str(), len);
	senddata[cpu_id][len] = 0;
}

void place_check_order(int fd, int cpu_id, map<string, string> &reqArgs)
{
	stringstream os;
	string tmpContent;
	char fileBuf[BUF_LEN];
	char *file_0 = NULL, *file_1 = NULL, *file_2 = NULL;

	map<string,string>::iterator iter = mySession.find(reqArgs["sessionID"]);
	if(iter == mySession.end())
	{
		file_0 = fileMap["login.tpl"];
		snprintf(fileBuf, BUF_LEN, file_0, "Please login!");
	}
	else
	{
		struct besimReturn returnData;
		os << "GET /api_dir/besim_api?1&11&" << iter->second << '&' << reqArgs["account_no"] << '&'
			<< "yyyymmdd" << '&' << reqArgs["number"] << " HTTP/1.1";
        string request = os.str();
		backend_get(fd, request, returnData);
		if (returnData.responseCode != OK_STATUS)
		{
	    	/*perror("besim place_check_order return error code");
	    	exit(-1);*/
	    	file_0 = fileMap["message.tpl"];
	    	snprintf(fileBuf, BUF_LEN, file_0, "besim place_check_order return error code: 1");
	    }
	    else
	    {
	    	file_0 = fileMap["place_check_order.tpl"];
			file_1 = fileMap["menu.tpl"];
			file_2 = fileMap["place_check_order"];

			snprintf(fileBuf, BUF_LEN, file_0, (iter->second).c_str(),
				"Check order placed successfully!", (returnData.rtnArgs[0][0]).c_str(),
				file_1, file_2);
	    }
	}

	os.str("");
	os << "HTTP/1.1 200 OK\r\nContent-Length: " << strlen(fileBuf)
		<< "\r\nConnection: keep-alive\r\n\r\n" << fileBuf;
	tmpContent = os.str();
	size_t len = tmpContent.length();
	memcpy(senddata[cpu_id], tmpContent.c_str(), len);
	senddata[cpu_id][len] = 0;
}

void transfer(int fd, int cpu_id, map<string, string> &reqArgs)
{
	stringstream os;
	string tmpContent;
	char fileBuf[BUF_LEN];
	char *file_0 = NULL, *file_1 = NULL, *file_2 = NULL;

	map<string,string>::iterator iter = mySession.find(reqArgs["sessionID"]);
	if(iter == mySession.end())
	{
		file_0 = fileMap["login.tpl"];
		snprintf(fileBuf, BUF_LEN, file_0, "Please login!");
	}
	else
	{
		struct besimReturn returnData;
		string request = "1&2&";
		request += iter->second;
        os.str("");
        os << "GET /api_dir/besim_api?" << request << " HTTP/1.1";
        request = os.str();
		backend_get(fd, request, returnData);
		if (returnData.responseCode != OK_STATUS)
		{
	    	perror("besim transfer return error code");
	    	exit(-1);
	    }

	    file_0 = fileMap["transfer.tpl"];

        os.str("");
	    for (int i = 0; i < returnData.argArrays; i++)
		{
		//	cout<<returnData.rtnArgs[i][1]<<endl;
			os << "<tr><td>" << returnData.rtnArgs[i][0] << "</td><td>";
			if (returnData.rtnArgs[i][1] == "01")
				os << "Checking";
			else if (returnData.rtnArgs[i][1] == "02")
				os << "Saving";
			else
				os << "Other";
			os << "</td><td>" << returnData.rtnArgs[i][2] << "</td></tr>";
		}
		string tmpstr = os.str();

		file_1 = fileMap["menu.tpl"];
		file_2 = fileMap["transfer"];

		snprintf(fileBuf, BUF_LEN, file_0, (iter->second).c_str(),
			tmpstr.c_str(), file_1, file_2);
	}

	os.str("");
	os << "HTTP/1.1 200 OK\r\nContent-Length: " << strlen(fileBuf)
		<< "\r\nConnection: keep-alive\r\n\r\n" << fileBuf;
	tmpContent = os.str();
	size_t len = tmpContent.length();
	memcpy(senddata[cpu_id], tmpContent.c_str(), len);
	senddata[cpu_id][len] = 0;
}

void post_transfer(int fd, int cpu_id, map<string, string> &reqArgs)
{
	stringstream os;
	string tmpContent;
	char fileBuf[BUF_LEN];
	char *file_0 = NULL, *file_1 = NULL, *file_2 = NULL;

	map<string,string>::iterator iter = mySession.find(reqArgs["sessionID"]);
	if(iter == mySession.end())
	{
		file_0 = fileMap["login.tpl"];
		snprintf(fileBuf, BUF_LEN, file_0, "Please login!");
	}
	else if (reqArgs["from"] == reqArgs["to"])
	{
		file_0 = fileMap["message.tpl"];
		snprintf(fileBuf, BUF_LEN, file_0, "Transfer from and to the same account not allowed!");
	}
	else
	{
		struct besimReturn returnData;
		os << "GET /api_dir/besim_api?1&12&" << iter->second << '&' << reqArgs["from"] << '&' << reqArgs["to"]
			<< '&' << reqArgs["amount"] << '&' << "20141225" << " HTTP/1.1";
		string request = os.str();
		backend_get(fd, request, returnData);
		if (returnData.responseCode != OK_STATUS)
		{
	    	/*perror("besim post_transfer return error code");
	    	exit(-1);*/
			file_0 = fileMap["message.tpl"];
	    	snprintf(fileBuf, BUF_LEN, file_0, "besim post_transfer return error code: 1");
	    }
	    else
	    {
	    	file_0 = fileMap["post_transfer.tpl"];

		    os.str("");
		    os << "<tr><td>" << returnData.rtnArgs[0][0] << "</td><td>" << returnData.rtnArgs[0][1]
		    	<< "</td></tr><tr><td>" << returnData.rtnArgs[1][0] << "</td><td>" << returnData.rtnArgs[1][1]
		    	<< "</td></tr>";
			string tmpstr = os.str();

			file_1 = fileMap["menu.tpl"];
			file_2 = fileMap["post_transfer"];

			snprintf(fileBuf, BUF_LEN, file_0, (iter->second).c_str(), reqArgs["from"].c_str(),
				reqArgs["to"].c_str(), reqArgs["amount"].c_str(), tmpstr.c_str(), file_1, file_2);
	    }
	}

	os.str("");
	os << "HTTP/1.1 200 OK\r\nContent-Length: " << strlen(fileBuf)
		<< "\r\nConnection: keep-alive\r\n\r\n" << fileBuf;
	tmpContent = os.str();
	size_t len = tmpContent.length();
	memcpy(senddata[cpu_id], tmpContent.c_str(), len);
	senddata[cpu_id][len] = 0;
}

void logout(int cpu_id, map<string, string> &reqArgs)
{
	stringstream os;
	string tmpContent, userid = reqArgs["userid"];
	char fileBuf[BUF_LEN];
	char *file_0 = NULL, *file_1 = NULL;

	map<string,string>::iterator iter = mySession.find(reqArgs["sessionID"]);
	if(iter != mySession.end())
	{
		userid = iter->second;
		mySession.erase(iter);
	}

	file_0 = fileMap["logout.tpl"];
	file_1 = fileMap["logout"];
	snprintf(fileBuf, BUF_LEN, file_0, userid.c_str(), file_1);
	os << "HTTP/1.1 200 OK\r\nContent-Length: " << strlen(fileBuf)
		<< "\r\nConnection: keep-alive\r\n\r\n" << fileBuf;
	tmpContent = os.str();
	size_t len = tmpContent.length();
	memcpy(senddata[cpu_id], tmpContent.c_str(), len);
	senddata[cpu_id][len] = 0;
}

void execute_script(int fd, int cpu_id, int commandType, map<string, string> &reqArgs)
{
//cout << commandType << endl;
	switch(commandType)
	{
		case LOGIN:
		{
			login(fd, cpu_id, reqArgs);
			break;
		}
		case ACCOUNT_SUMMARY:
		{
			account_summary(fd, cpu_id, reqArgs);
			break;
		}
		case CHECK_DETAIL_HTML:
		{
			check_detail_html(fd, cpu_id, reqArgs);
			break;
		}
		case BILL_PAY:
		{
			bill_pay(fd, cpu_id, reqArgs);
			break;
		}
		case ADD_PAYEE:
		{
			add_payee(cpu_id, reqArgs);
			break;
		}
		case POST_PAYEE:
		{
			post_payee(fd, cpu_id, reqArgs);
			break;
		}
		case QUICK_PAY:
		{
			quick_pay(fd, cpu_id, reqArgs);
			break;
		}
		case BILL_PAY_STATUS_OUTPUT:
		{
			bill_pay_status_output(fd, cpu_id, reqArgs);
			break;
		}
		case PROFILE:
		{
			profile(fd, cpu_id, reqArgs);
			break;
		}
		case CHANGE_PROFILE:
		{
			change_profile(fd, cpu_id, reqArgs);
			break;
		}
		case ORDER_CHECK:
		{
			order_check(fd, cpu_id, reqArgs);
			break;
		}
		case PLACE_CHECK_ORDER:
		{
			place_check_order(fd, cpu_id, reqArgs);
			break;
		}
		case TRANSFER:
		{
			transfer(fd, cpu_id, reqArgs);
			break;
		}
		case POST_TRANSFER:
		{
			post_transfer(fd, cpu_id, reqArgs);
			break;
		}
		case LOGOUT:
		{
			logout(cpu_id, reqArgs);
			break;
		}
		case CHECK_DETAIL_IMAGE:
		{
			check_detail_image(fd, cpu_id, reqArgs);
			break;
		}
		case FILE_200:
		{
			file_200(cpu_id, reqArgs);
			break;
		}
		case FILE_304:
		{
			file_304(cpu_id, reqArgs);
			break;
		}
		case INIT:
		{
			init(cpu_id);
			break;
		}
	}
}
