// This autogenerated skeleton file illustrates how to build a server.
// You should copy it to another filename to avoid overwriting it.

#include "SimilarityJoinService.h"
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/concurrency/ThreadManager.h>
#include <thrift/concurrency/PosixThreadFactory.h>
#include <thrift/server/TThreadPoolServer.h>
#include <thrift/server/TThreadedServer.h>

#include "SecureJoin.h"

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

using boost::shared_ptr;

class SimilarityJoinServiceHandler : virtual public SimilarityJoinServiceIf {
protected:

	SecureJoin joinEngine;
	uint32_t userCount;


public:
	SimilarityJoinServiceHandler()
	{
		srand(0);
		string path = "financeNormalize.data";
		userCount = joinEngine.loadData(path);
		cout << "Load " << userCount << " data from " << path << endl;
		//L=250 w=1.5 R=1.21 r=1.1 K=88
		joinEngine.computeLSH(250, 1.5);
		joinEngine.buildIndex(userCount);
	}

	void QueryDataByID(std::vector<std::string> & _return, const std::vector<int32_t> & UserIDs)
	{
		for (auto i = UserIDs.begin(); i != UserIDs.end(); i++)
		{
			_return.push_back(joinEngine.getMataDataByID(*i));
		}
	}

	void QueryTypeByID(std::vector<std::string> & _return, const std::vector<int32_t> & UserIDs)
	{
		for (auto i = UserIDs.begin(); i != UserIDs.end(); i++)
		{
			_return.push_back(joinEngine.getTypeByID(*i));
		}
	}

	void QueryDistributedByID(std::vector<int32_t> & _return, const std::vector<int32_t> & UserIDs)
	{
		vector<uint32_t> unUserIDs;
		for (auto i = UserIDs.begin(); i != UserIDs.end(); i++)
		{
			unUserIDs.push_back(*i);
		}
		SecureJoin::Proportion pro = joinEngine.getDistributedByID(unUserIDs);
		for (int i = 0; i < 4; i++)
		{
			_return.push_back(pro.counter[i]);
		}
	}

	void QueryTypeByData(std::vector<std::string> & _return, const std::vector<std::string> & Datas)
	{
		int dimension = joinEngine.uiFinanceDimension;
		double* buffer = new double[dimension];
		vector<string> tmpVecStr;

		for (auto i = Datas.begin(); i != Datas.end(); i++)
		{
			tmpVecStr.clear();
			joinEngine.splitString(*i, tmpVecStr, " ");
			memset(buffer, 0, sizeof(double)*dimension);
			for (int j = 0; j < dimension && j < tmpVecStr.size(); j++)
			{
				buffer[j] = stod(tmpVecStr[j]);
			}
			_return.push_back(joinEngine.getTypeByData(buffer));
		}

		delete[] buffer;
	}

	void GetIndexDistributed(std::vector<int32_t> & _return) {
		SecureJoin::Proportion pro = joinEngine.indexDistributed;
		for (int i = 0; i < 4; i++)
		{
			_return.push_back(pro.counter[i]);
		}
	}

	void GetIndexMetaData(std::vector<std::string> & _return)
	{
		_return.push_back(to_string(joinEngine.uiUserNum));
		_return.push_back(to_string(joinEngine.indexSize));
		//2142639360 bytes
		_return.push_back(to_string(joinEngine.indexMomery / 1024 / 1024));
		//16433180Kb memory
		_return.push_back(to_string(16433180 / 1024));
	}

	void GetLatestQueryTime(std::vector<std::string> & _return)
	{
		_return.push_back(to_string(joinEngine.perQueryTimerSelfQuery));
		_return.push_back(to_string(joinEngine.perQueryTimerComputeLSH));
		_return.push_back(to_string(joinEngine.perQueryTimerComputeToken));
		_return.push_back(to_string(joinEngine.perQueryTimerGetData));
		_return.push_back(to_string(joinEngine.perQueryTimerFilter));
		_return.push_back(to_string(joinEngine.perQueryTimerQuery));
	}

	void JoinByStrategy1(std::vector<int32_t> & _return, const std::vector<std::string> & Datas, const int32_t ThresholdK, const int32_t TimeOut) {
		int dimension = joinEngine.uiDataDimension;
		uint32_t querySize = Datas.size();
		if (querySize < 1)
		{
			return;
		}
		double** buffer = new double*[querySize];
		for (int i = 0; i < querySize; i++)
		{
			buffer[i] = new double[dimension];
		}

		vector<string> tmpVecStr;
		for (int i = 0; i < querySize; i++)
		{
			tmpVecStr.clear();
			joinEngine.splitString(Datas[i], tmpVecStr, " ");
			memset(buffer[i], 0, sizeof(double)*dimension);
			for (int j = 0; j < dimension&&j < tmpVecStr.size(); j++)
			{
				buffer[i][j] = stod(tmpVecStr[j]);
			}
			joinEngine.normalize(buffer[i], dimension);
		}

		_return = joinEngine.joinByStrategy1(buffer, querySize, ThresholdK);

		for (int i = 0; i < Datas.size(); i++)
		{
			delete[]buffer[i];
		}
		delete[] buffer;
	}

	void JoinByStrategy2(std::vector<int32_t> & _return, const std::vector<std::string> & Datas, const int32_t ThresholdK, const int32_t TimeOut) {
		int dimension = joinEngine.uiDataDimension;
		uint32_t querySize = Datas.size();
		if (querySize < 1)
		{
			return;
		}
		double** buffer = new double*[querySize];
		for (int i = 0; i < querySize; i++)
		{
			buffer[i] = new double[dimension];
		}

		vector<string> tmpVecStr;
		for (int i = 0; i < querySize; i++)
		{
			tmpVecStr.clear();
			joinEngine.splitString(Datas[i], tmpVecStr, " ");
			memset(buffer[i], 0, sizeof(double)*dimension);
			for (int j = 0; j < dimension&&j < tmpVecStr.size(); j++)
			{
				buffer[i][j] = stod(tmpVecStr[j]);
			}
			joinEngine.normalize(buffer[i], dimension);
		}

		_return = joinEngine.joinByStrategy2(buffer, Datas.size(), ThresholdK);

		for (int i = 0; i < Datas.size(); i++)
		{
			delete[]buffer[i];
		}
		delete[] buffer;
	}

	void JoinByStrategy3(std::vector<int32_t> & _return, const std::vector<std::string> & Datas, const int32_t ThresholdK, const int32_t TimeOut, const double SelfQueryR) {
		int dimension = joinEngine.uiDataDimension;
		uint32_t querySize = Datas.size();
		if (querySize < 1)
		{
			return;
		}
		double** buffer = new double*[querySize];
		for (int i = 0; i < querySize; i++)
		{
			buffer[i] = new double[dimension];
		}

		vector<string> tmpVecStr;
		for (int i = 0; i < querySize; i++)
		{
			tmpVecStr.clear();
			joinEngine.splitString(Datas[i], tmpVecStr, " ");
			memset(buffer[i], 0, sizeof(double)*dimension);
			for (int j = 0; j < dimension&&j < tmpVecStr.size(); j++)
			{
				buffer[i][j] = stod(tmpVecStr[j]);
			}
			joinEngine.normalize(buffer[i], dimension);
		}

		_return = joinEngine.joinByStrategy3(buffer, Datas.size(), ThresholdK, SelfQueryR);

		for (int i = 0; i < Datas.size(); i++)
		{
			delete[]buffer[i];
		}
		delete[] buffer;
	}

};

int main(int argc, char **argv) {
	int port = 9090;
	boost::shared_ptr<SimilarityJoinServiceHandler> handler(new SimilarityJoinServiceHandler());
	boost::shared_ptr<TProcessor> processor(new SimilarityJoinServiceProcessor(handler));
	boost::shared_ptr<TServerTransport> serverTransport(new TServerSocket(port));
	boost::shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
	boost::shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());

	TSimpleServer server(processor, serverTransport, transportFactory, protocolFactory);
	server.serve();
	return 0;
}


//int main(int argc, char **argv) {
//	int port = 9090;
//
//	boost::shared_ptr<SimilarityJoinServiceHandler> handler(new SimilarityJoinServiceHandler());
//	boost::shared_ptr<TProcessor> processor(new SimilarityJoinServiceProcessor(handler));
//	boost::shared_ptr<TServerTransport> serverTransport(new TServerSocket(port));
//	boost::shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
//	boost::shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());
//
//
//	//#define DEF_USE_THREADPOOL
//#ifdef DEF_USE_THREADPOOL
//	const int workerCount = 500;
//	boost::shared_ptr<ThreadManager> threadManager = ThreadManager::newSimpleThreadManager(workerCount);
//	boost::shared_ptr<PosixThreadFactory> threadFactory = boost::shared_ptr<PosixThreadFactory>(new PosixThreadFactory());
//	threadManager->threadFactory(threadFactory);
//	threadManager->start();
//	TThreadPoolServer server(processor, serverTransport, transportFactory, protocolFactory, threadManager);
//#else
//	TThreadedServer server(processor, serverTransport, transportFactory, protocolFactory);
//#endif
//	try
//	{
//		server.serve();
//	}
//	catch (TException& tx) {
//		cout << "ERROR: " << tx.what() << endl;
//	}
//
//	return 0;
//}
//
//
