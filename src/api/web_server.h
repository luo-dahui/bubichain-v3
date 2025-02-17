#ifndef WEB_SERVER_H_
#define WEB_SERVER_H_

#include <3rd/http/server.hpp>
#include <common/general.h>
#include <common/storage.h>
#include <common/pb2json.h>
#include <proto/cpp/chain.pb.h>
#include <utils/singleton.h>
#include <utils/net.h>
#include <main/configure.h>

namespace bubi {

	class WebServer :public utils::Singleton<bubi::WebServer>, public bubi::StatusModule
	{
		friend class utils::Singleton<bubi::WebServer>;
	public:
		WebServer();
		~WebServer();
	private:
		utils::AsyncIo *async_io_ptr_;
		http::server::server *server_ptr_;
		asio::ssl::context *context_;
		bool running;
		size_t thread_count_;

		void FileNotFound(const http::server::request &request, std::string &reply);
		void Hello(const http::server::request &request, std::string &reply);
		void CreateAccount(const http::server::request &request, std::string &reply);
		void GetAccount(const http::server::request &request, std::string &reply);
		void GetGenesisAccount(const http::server::request &request, std::string &reply);
		void GetAccountMetaData(const http::server::request &request, std::string &reply);
		void GetAccountAssets(const http::server::request &request, std::string &reply);

		void Debug(const http::server::request &request, std::string &reply);

		void CreateTransaction(const http::server::request &request, std::string &reply);
		void GetTransactionBlob(const http::server::request &request, std::string &reply);
		void UpdateLogLevel(const http::server::request &request, std::string &reply);

		void GetTransactionHistory(const http::server::request &request, std::string &reply);
		void GetContractTx(const http::server::request &request, std::string &reply);

		//void GetRecord(const http::server::request &request, std::string &reply);
		void GetStatus(const http::server::request &request, std::string &reply);
		void GetModulesStatus(const http::server::request &request, std::string &reply);
		void GetLedger(const http::server::request &request, std::string &reply);
		void GetLedgerValidators(const http::server::request &request, std::string &reply);
		void GetAddress(const http::server::request &request, std::string &reply);
		void GetPeerNodeAddress(const http::server::request &request, std::string &reply);
		void GetTransactionFromBlob(const http::server::request &request, std::string &reply);
		void GetExprResult(const http::server::request &request, std::string &reply);
		void GetPeerAddresses(const http::server::request &request, std::string &reply);

		void GetConsensusInfo(const http::server::request &request, std::string &reply);

		std::string GetCertPassword(std::size_t, asio::ssl::context_base::password_purpose purpose);

		void MultiQuery(const http::server::request &request, std::string &reply);
		void SubmitTransaction(const http::server::request &request, std::string &reply);
		void ConfValidator(const http::server::request &request, std::string &reply);

		bool MakeTransactionHelper(const Json::Value &object, protocol::Transaction *tran, Result& result);

	public:
		bool Initialize(WebServerConfigure &webserver_configure);
		bool Exit();
		void GetModuleStatus(Json::Value &data);
	};
}

#endif
