#include <map>
#include <vector>
#include <sstream>
#include <fstream>
#include <sys/stat.h>

using namespace std;

#define LOGIN 0
#define ACCOUNT_SUMMARY 1
#define CHECK_DETAIL_HTML 2
#define BILL_PAY 3
#define ADD_PAYEE 4
#define POST_PAYEE 5
#define QUICK_PAY 6
#define BILL_PAY_STATUS_OUTPUT 7
#define PROFILE 8
#define CHANGE_PROFILE 9
#define ORDER_CHECK 10
#define PLACE_CHECK_ORDER 11
#define TRANSFER 12
#define POST_TRANSFER 13
#define LOGOUT 14
#define CHECK_DETAIL_IMAGE 15
#define FILE_200 16
#define FILE_304 17
#define INIT 18

#define OK_STATUS 0

struct besimReturn
{
	int responseCode;
	int argArrays;
	vector<string> rtnArgs[20];
};

void process_besim(int fd, int cpu_id);
int parse_client_request(string &str, map<string, string> &reqArgs);
void getArgs(int commandType, string &str, map<string, string> &reqArgs);
void execute_script(int fd, int cpu_id, int commandType, map<string, string> &reqArgs);

void backend_get(int fd, string &request, struct besimReturn& returnData);

void login(int fd, int cpu_id, map<string, string> &reqArgs);
void account_summary(int fd, int cpu_id, map<string, string> &reqArgs);
void check_detail_html(int fd, int cpu_id, map<string, string> &reqArgs);
void bill_pay(int fd, int cpu_id, map<string, string> &reqArgs);
void add_payee(int cpu_id, map<string, string> &reqArgs);
void post_payee(int fd, int cpu_id, map<string, string> &reqArgs);
void quick_pay(int fd, int cpu_id, map<string, string> &reqArgs);
void bill_pay_status_output(int fd, int cpu_id, map<string, string> &reqArgs);
void profile(int fd, int cpu_id, map<string, string> &reqArgs);
void change_profile(int fd, int cpu_id, map<string, string> &reqArgs);
void order_check(int fd, int cpu_id, map<string, string> &reqArgs);
void place_check_order(int fd, int cpu_id, map<string, string> &reqArgs);
void transfer(int fd, int cpu_id, map<string, string> &reqArgs);
void post_transfer(int fd, int cpu_id, map<string, string> &reqArgs);
void logout(int cpu_id, map<string, string> &reqArgs);
void check_detail_image(int fd, int cpu_id, map<string, string> &reqArgs);
void file_200(int cpu_id, map<string, string> &reqArgs);
void file_304(int cpu_id, map<string, string> &reqArgs);
void init(int cpu_id);
