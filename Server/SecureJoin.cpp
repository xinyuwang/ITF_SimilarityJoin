#include <fstream>
#include <map>
#include <time.h>
#include <sstream>
#include <sys/shm.h>
#include <sys/time.h>

#include "SecureJoin.h"
#include "../Caravel/BukHash.h"
#include "../Caravel/ShmCtl.h"
#include "../Caravel/PRF.h"


using namespace std;
using namespace caravel;

SecureJoin::SecureJoin()
{
	init();
}

SecureJoin::~SecureJoin()
{
	free();
}

int SecureJoin::init()
{
	arMaxBoundry = new double[uiFinanceDimension];
	vector<string> splitRes;
	splitString(strMaxBoundry, splitRes, " ");
	for (int i = 0; i < uiFinanceDimension; i++)
	{
		if (i < splitRes.size())
		{
			arMaxBoundry[i] = stod(splitRes[i]);
		}
		else
		{
			arMaxBoundry[i] = 0;
		}
	}

	encIndex.SetKey(masterKey);

	return 0;
}

int SecureJoin::free()
{
	checkAndDeleteArr((void**)&arMaxBoundry);
	if (arMetaVal != NULL)
	{
		for (int i = 0; i < uiAllNum; i++)
		{
			checkAndDeleteArr((void**)&arMetaVal[i]);
		}
		delete[] arMetaVal;
		arMetaVal = NULL;
	}
	return 0;
}

uint32_t SecureJoin::loadData(string mataDataPaht, uint32_t maxSize)
{
	uint32_t tmpAllNum = countLines(mataDataPaht);
	uint32_t tmpDimension = countDimension(mataDataPaht);

	ifstream ifs;
	ifs.open(mataDataPaht, ios::in);
	string strLines;

	if (ifs.fail() || uiAllNum < 0 || uiDataDimension < 0)
	{
		cout << "The file did not exists." << endl;
		return -1;
	}
	else
	{
		uiAllNum = tmpAllNum;
		uiDataDimension = tmpDimension;
		indexDistributed.total = uiAllNum;

		arMetaVal = new double*[uiAllNum];
		for (int i = 0; i < uiAllNum; i++)
		{
			arMetaVal[i] = new double[uiDataDimension];
			memset(arMetaVal[i], 0, sizeof(double)*uiDataDimension);
		}

		vector<string> splitRes;
		uint32_t uiCurNum = 0;
		while (getline(ifs, strLines, '\n'))
		{
			splitRes.clear();
			splitString(strLines, splitRes, " ");
			for (int i = 0; i < splitRes.size() && i < uiDataDimension; i++)
			{
				try
				{
					arMetaVal[uiCurNum][i] = stod(splitRes[i]);
				}
				catch (invalid_argument e)
				{
					cout << splitRes[i] << "can`t be convert to double" << endl;
					arMetaVal[uiCurNum][i] = 0;
				}
			}
			indexDistributed.counter[checkLevel(arMetaVal[uiCurNum])]++;
			uiCurNum++;
		}
		ifs.close();
		return tmpAllNum;
	}
}

bool SecureJoin::computeLSH(uint32_t L, double w)
{
	if (arMetaVal == NULL)
	{
		return false;
	}
	uiLshL = L;
	dLshW = w;

	c2lsh.Init(uiDataDimension, uiLshL, dLshW);
	arLsh = new uint32_t*[uiAllNum];

	for (uint32_t uiCur = 0; uiCur < uiAllNum; uiCur++)
	{
		arLsh[uiCur] = new uint32_t[uiLshL];
		c2lsh.Compute(arMetaVal[uiCur], arLsh[uiCur]);
	}

	return true;
}

bool SecureJoin::computeLsh(uint32_t * lsh, double * mateData)
{
	if (lsh != NULL&&mateData != NULL)
	{
		c2lsh.Compute(mateData, lsh);
		return true;
	}

	return false;
}

bool SecureJoin::buildIndex(uint32_t size)
{
	if (arLsh == NULL)
	{
		return false;
	}
	uiUserNum = size;

	int iShmID = shmget(shmKey, 0, 0);
	if (iShmID >= 0)
	{
		encIndex.AttachIndex(uiUserNum * uiLshL, 0.7, 24, shmKey);
	}
	else
	{
		encIndex.Init(uiUserNum * uiLshL, 0.7, 24, shmKey);

		encIndex.BuildIndex(arLsh, uiUserNum, uiLshL);
	}


	encIndex.ShowBukHashState();
	indexSize = encIndex.getIndexSize() / uiLshL;
	indexMomery = encIndex.getIndexSize() * (sizeof(uint64_t) + sizeof(BukEncBlock));

	return true;
}

string SecureJoin::getMataDataByID(uint32_t id)
{
	stringstream ss;
	if (id < uiAllNum)
	{
		for (int i = 0; i < uiDataDimension; i++)
		{
			ss << arMetaVal[id][i] * arMaxBoundry[i];
			if (i != uiDataDimension - 1)
			{
				ss << " ";
			}
		}
	}
	return ss.str();
}

string SecureJoin::getTypeByID(uint32_t id)
{
	string level;
	if (id < uiAllNum)
	{
		int lvl = checkLevel(arMetaVal[id]);
		level.append(1, Type2c(lvl));
	}
	return level;
}

string SecureJoin::getTypeByData(double * oneMateData)
{
	string level;
	level.append(1, Type2c(checkLevel(oneMateData)));
	return level;
}

SecureJoin::Proportion SecureJoin::getDistributedByID(vector<uint32_t> ids)
{
	set<int> setIDs;
	setIDs.insert(ids.begin(), ids.end());
	return countLevel(arMetaVal, ids);
}

SecureJoin::Proportion SecureJoin::getDistributedofIndex()
{
	return indexDistributed;
}

vector<int> SecureJoin::joinByStrategy1(double ** joinMataData, int num, int ThresholdK, int timeout)
{
	perQueryTimerSelfQuery = 0;
	perQueryTimerComputeLSH = 0;
	perQueryTimerComputeToken = 0;
	perQueryTimerGetData = 0;
	perQueryTimerFilter = 0;
	perQueryTimerQuery = 0;

	uint32_t uiJoinNum = num;
	uint64_t ulNeedBandwidthNum = 0;
	uint32_t uiLimitK = ThresholdK;
	set<uint32_t> setResult;
	markSecond();
	markMicroSecond(1);
	uint32_t **queryLsh = new uint32_t*[uiJoinNum];

	for (uint32_t uiCur = 0; uiCur < uiJoinNum; uiCur++)
	{
		queryLsh[uiCur] = new uint32_t[uiLshL];

		markMicroSecond();
		computeLsh(queryLsh[uiCur], joinMataData[uiCur]);
		perQueryTimerComputeLSH += markMicroSecond();

		uint32_t *arQueryLsh = queryLsh[uiCur];

		vector<uint32_t> vecResult;
		for (uint32_t uiL = 0; uiL < uiLshL; uiL++)
		{
			uint32_t uiLsh = arQueryLsh[uiL];
			encIndex.QueryOne(uiLsh, uiL, vecResult,&perQueryTimerComputeToken, &perQueryTimerGetData);
		}
		map<uint32_t, uint32_t> mapCombine;
		vector<uint32_t> vecResultInK;

		markMicroSecond();
		for (auto it = vecResult.begin(); it != vecResult.end(); it++)
		{
			if (++mapCombine[*it] == uiLimitK)
			{
				vecResultInK.push_back(*it);
			}
		}
		perQueryTimerFilter += markMicroSecond();

		ulNeedBandwidthNum += vecResultInK.size();
		setResult.insert(vecResultInK.begin(), vecResultInK.end());

		if ((timeout -= markSecond()) < 0)
		{
			break;
		}

	}

	cout << "ulNeedBandwidthNum = " << ulNeedBandwidthNum << endl;

	vector<int> vecRes;
	for (auto i = setResult.begin(); i != setResult.end(); i++)
	{
		vecRes.push_back((int)*i);
	}

	for (int i = 0; i < uiJoinNum; i++)
	{
		delete[] queryLsh[i];
	}
	delete[] queryLsh;

	perQueryTimerQuery += markMicroSecond(1);
	return vecRes;

}

vector<int> SecureJoin::joinByStrategy2(double ** joinMataData, int num, int ThresholdK, int timeout)
{
	//promise that if join 3 use join 2, it will make perQueryTimerSelfQuery a negative num.
	if (perQueryTimerSelfQuery >= 0)
	{
		perQueryTimerSelfQuery = 0;
		perQueryTimerComputeLSH = 0;
		perQueryTimerComputeToken = 0;
		perQueryTimerGetData = 0;
		perQueryTimerFilter = 0;
		perQueryTimerQuery = 0;
	}

	uint32_t uiJoinNum = num;
	uint32_t uiLimitK = ThresholdK;

	uint64_t ulNeedBandwidthNum = 0;
	uint32_t uiHitCache = 0;
	uint64_t ulSaveResultNum = 0;

	map<uint32_t, map<uint32_t, vector<uint32_t> > >mapCacheResult;
	set<uint32_t> setResult;
	markSecond();
	if (perQueryTimerSelfQuery >= 0)
	{
		markMicroSecond(1);
	}

	uint32_t **queryLsh = new uint32_t*[uiJoinNum];

	for (uint32_t uiCur = 0; uiCur < uiJoinNum; uiCur++)
	{
		queryLsh[uiCur] = new uint32_t[uiLshL];

		markMicroSecond();
		computeLsh(queryLsh[uiCur], joinMataData[uiCur]);
		perQueryTimerComputeLSH += markMicroSecond();

		uint32_t *arQueryLsh = queryLsh[uiCur];
		map<uint32_t, uint32_t> mapCombine;
		vector<uint32_t> vecResultInK;

		for (uint32_t uiL = 0; uiL < uiLshL; uiL++)
		{
			uint32_t uiLsh = arQueryLsh[uiL];
			vector<uint32_t> *pVecReslt = &mapCacheResult[uiLsh][uiL];

			if (pVecReslt->size() != 0)
			{
				uiHitCache++;
				ulSaveResultNum += pVecReslt->size();
			}
			else
			{
				encIndex.QueryOne(uiLsh, uiL, *pVecReslt, &perQueryTimerComputeToken, &perQueryTimerGetData);
				ulNeedBandwidthNum += pVecReslt->size();
			}
			markMicroSecond();
			for (vector<uint32_t>::iterator it = pVecReslt->begin(); it != pVecReslt->end(); it++)
			{
				if (++mapCombine[*it] == uiLimitK)
				{
					vecResultInK.push_back(*it);
				}
			}
			perQueryTimerFilter+=markMicroSecond();
		}

		setResult.insert(vecResultInK.begin(), vecResultInK.end());

		if ((timeout -= markSecond()) < 0)
		{
			break;
		}

	}
	cout << "ulNeedBandwidthNum = " << ulNeedBandwidthNum << endl;
	cout << "uiHitCache = " << uiHitCache << endl;
	cout << "ulSaveResultNum = " << ulSaveResultNum << endl;

	vector<int> vecRes;
	for (auto i = setResult.begin(); i != setResult.end(); i++)
	{
		vecRes.push_back((int)*i);
	}

	for (int i = 0; i < uiJoinNum; i++)
	{
		delete[]queryLsh[i];
	}
	delete[]queryLsh;
	if (perQueryTimerSelfQuery >= 0)
	{
		perQueryTimerQuery += markMicroSecond(1);
	}
	return vecRes;
}

vector<int> SecureJoin::joinByStrategy3(double ** joinMataData, int num, int ThresholdK, double selfQueryR, int timeout)
{
	perQueryTimerSelfQuery = 0;
	perQueryTimerComputeLSH = 0;
	perQueryTimerComputeToken = 0;
	perQueryTimerGetData = 0;
	perQueryTimerFilter = 0;
	perQueryTimerQuery = 0;

	markMicroSecond(1);

	uint32_t uiJoinNum = num;
	double dSelfQueryR = selfQueryR;

	uint32_t **queryLsh = new uint32_t*[uiJoinNum];
	vector<uint32_t> vecJoin;
	for (uint32_t uiCur = 0; uiCur < uiJoinNum; uiCur++)
	{
		queryLsh[uiCur] = new uint32_t[uiLshL];
		markMicroSecond();
		computeLsh(queryLsh[uiCur], joinMataData[uiCur]);
		perQueryTimerComputeLSH += markMicroSecond();

		vecJoin.push_back(uiCur);
	}

	vector<uint32_t> vecSelfQuery;
	markMicroSecond();
	while (vecJoin.size() != 0)
	{
		uint32_t uiOffset = rand() % vecJoin.size();
		uint32_t uiQuerySample = vecJoin[uiOffset];

		vecJoin.erase(vecJoin.begin() + uiOffset);
		vecSelfQuery.push_back(uiQuerySample);
		for (vector<uint32_t>::iterator it = vecJoin.begin(); it != vecJoin.end();)
		{
			if (C2Lsh::ComputeL2(joinMataData[uiQuerySample], joinMataData[*it], uiDataDimension) < dSelfQueryR)
			{
				it = vecJoin.erase(it);
				if (it == vecJoin.end())
				{
					break;
				}
			}
			else
			{
				it++;
			}
		}
	}
	perQueryTimerSelfQuery -= markMicroSecond();
	uint32_t uiSelfQueryNum = vecSelfQuery.size();
	cout << "After selfQuery the join num is " << uiSelfQueryNum <<endl;


	double ** arrSelfQueryMataData = new double *[uiSelfQueryNum];
	for (int i = 0; i < uiSelfQueryNum; i++)
	{
		arrSelfQueryMataData[i] = new double[uiDataDimension];
		memcpy(arrSelfQueryMataData[i], joinMataData[vecSelfQuery[i]], sizeof(double)*uiDataDimension);
	}

	//After get selfQuery result we use strategy2 to complete query operation.
	vector<int> res = joinByStrategy2(arrSelfQueryMataData, uiSelfQueryNum, ThresholdK, timeout);

	for (int i = 0; i < uiJoinNum; i++)
	{
		delete[]queryLsh[i];
	}
	delete[]queryLsh;
	for (int i = 0; i < vecSelfQuery.size(); i++)
	{
		delete[]arrSelfQueryMataData[i];
	}
	delete[]arrSelfQueryMataData;

	perQueryTimerQuery += markMicroSecond(1);
	perQueryTimerSelfQuery = -perQueryTimerSelfQuery;

	return res;
}

int SecureJoin::countLines(string sPath)
{
	ifstream ifs;
	ifs.open(sPath, ios::in);
	int iLines = 0;
	string strLines;
	if (ifs.fail())
	{
		return -1;
	}
	else
	{
		while (getline(ifs, strLines, '\n'))
		{
			iLines++;
		}
	}
	ifs.close();
	return iLines;
}

int SecureJoin::countDimension(string sPath)
{
	ifstream ifs;
	ifs.open(sPath, ios::in);
	int iDimension = 0;
	string strLines;
	if (ifs.fail())
	{
		return -1;
	}
	else
	{
		getline(ifs, strLines, '\n');
		vector<string> splitRes;
		splitString(strLines, splitRes, " ");
		iDimension = splitRes.size();
		ifs.close();
		return iDimension;
	}
}

int SecureJoin::splitString(const std::string& s, std::vector<std::string>& v, const std::string& c, bool notNull)
{
	std::string::size_type pos1, pos2;
	pos1 = 0;
	pos2 = s.find(c, pos1);
	while (std::string::npos != pos2)
	{
		if (!notNull || (pos2 - pos1) > 0)
			v.push_back(s.substr(pos1, pos2 - pos1));

		pos1 = pos2 + c.size();
		pos2 = s.find(c, pos1);
	}
	if (pos1 < s.length())
		v.push_back(s.substr(pos1));

	return v.size();
}

void SecureJoin::normalize(double * arMetaData, uint32_t uiDimension)
{
	if (arMetaData != NULL&&arMetaData[0] > 1)
	{
		for (uint32_t uiD = 0; uiD < uiDimension; uiD++)
		{
			arMetaData[uiD] = arMetaData[uiD] / arMaxBoundry[uiD];
		}
	}
}

void SecureJoin::renormalize(double * arMetaData, uint32_t uiDimension)
{
	if (arMetaData != NULL&&arMetaData[0] < 1)
	{
		for (uint32_t uiD = 0; uiD < uiDimension; uiD++)
		{
			arMetaData[uiD] = arMetaData[uiD] * arMaxBoundry[uiD];
		}
	}
}

void SecureJoin::formalize(double ** arMetaData, uint32_t uiAllNum, uint32_t uiDimension)
{
	double* maxValue = new double[uiDimension];
	double* minValue = new double[uiDimension];

	for (uint32_t uiD = 0; uiD < uiDimension; uiD++)
	{
		maxValue[uiD] = -(int64_t)UINT32_MAX;
		minValue[uiD] = (int64_t)UINT32_MAX;
	}

	for (uint32_t uiCur = 0; uiCur < uiAllNum; uiCur++)
	{
		for (uint32_t uiD = 0; uiD < uiDimension; uiD++)
		{
			maxValue[uiD] = maxValue[uiD] > arMetaData[uiCur][uiD] ? maxValue[uiD] : arMetaData[uiCur][uiD];
			minValue[uiD] = minValue[uiD] < arMetaData[uiCur][uiD] ? minValue[uiD] : arMetaData[uiCur][uiD];
		}
	}

	for (uint32_t uiCur = 0; uiCur < uiAllNum; uiCur++)
	{
		double *arVal = arMetaData[uiCur];
		for (uint32_t uiD = 0; uiD < uiDimension; uiD++)
		{
			arVal[uiD] = (arVal[uiD] - minValue[uiD]) / (maxValue[uiD] - minValue[uiD]);
		}
	}

	delete[]maxValue;
	delete[]minValue;
}

void SecureJoin::checkAndDeleteArr(void** p)
{
	if (p != nullptr && *p != nullptr)
	{
		delete[] * p;
		*p = nullptr;
	}
}

int SecureJoin::checkLevel(double* pMateData)
{
	//not normalize
	if (pMateData[0] > 1 && pMateData[1] > 1)
	{
		if (pMateData[1] < 10000)
		{
			return 0;
		}
		else if (pMateData[1] < 25000)
		{
			return 1;
		}
		else if (pMateData[1] < 50000)
		{
			return 2;
		}
		else
		{
			return 3;
		}
	}
	else
	{
		if (pMateData[1] < 10000 / 75000.0)
		{
			return 0;
		}
		else if (pMateData[1] < 25000 / 75000.0)
		{
			return 1;
		}
		else if (pMateData[1] < 50000 / 75000.0)
		{
			return 2;
		}
		else
		{
			return 3;
		}
	}
}

int SecureJoin::checkLevel(double** arMateData, uint32_t mateDataID)
{
	return checkLevel(arMateData[mateDataID]);
}

SecureJoin::Proportion SecureJoin::countLevel(double** pArrMateData, uint32_t datacount)
{
	Proportion res;
	res.total = datacount;
	for (int i = 0; i < datacount; i++)
	{
		int lvl = checkLevel(pArrMateData[i]);
		res.counter[lvl]++;
	}
	return res;
}

SecureJoin::Proportion SecureJoin::countLevel(double** pArrMateData, const vector<uint32_t>& ids)
{
	Proportion res;
	res.total = ids.size();
	for (int i = 0; i < ids.size(); i++)
	{
		if (ids[i] >= uiAllNum)
		{
			continue;
		}
		int lvl = checkLevel(pArrMateData, ids[i]);
		res.counter[lvl]++;
	}
	return res;
}

SecureJoin::Proportion SecureJoin::countLevel(double** pArrMateData, const set<uint32_t>& ids)
{
	Proportion res;
	res.total = ids.size();
	for (auto i = ids.begin(); i != ids.end(); i++)
	{
		if (*i >= uiAllNum)
		{
			continue;
		}
		int lvl = checkLevel(pArrMateData, *i);
		res.counter[lvl]++;
	}
	return res;
}

char SecureJoin::Type2c(int lvl)
{
	return (char)(lvl + 'A');
}

uint32_t SecureJoin::markSecond(int id)
{
	static time_t t_cur[16];

	if (id < 0 || id>15)
	{
		for (int i = 0; i < 16; i++)
		{
			t_cur[id] = time(NULL);
		}
		return 0;
	}

	uint32_t ui_Time = time(NULL) - t_cur[id];
	t_cur[id] = time(NULL);

	return ui_Time;
}

uint32_t SecureJoin::markMicroSecond(int id)
{
	static timeval t_start[16];
	static timeval t_end[16];
	
	if (id < 0 || id>15)
	{
		for (int i = 0; i < 16; i++)
		{
			gettimeofday(&t_start[i], NULL);
		}
		return 0;
	}

	gettimeofday(&t_end[id], NULL);
	uint32_t uiTimeInterval = 1000000 * (t_end[id].tv_sec - t_start[id].tv_sec) + 
		t_end[id].tv_usec - t_start[id].tv_usec;
	gettimeofday(&t_start[id], NULL);

	return uiTimeInterval;
}
