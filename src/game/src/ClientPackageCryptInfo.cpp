#include "stdafx.h"
#include "ClientPackageCryptInfo.h"
#include <common/stl.h>

CClientPackageCryptInfo::CClientPackageCryptInfo() : m_pSerializedCryptKeyStream(NULL), m_nCryptKeyPackageCnt(0)
{
}

CClientPackageCryptInfo::~CClientPackageCryptInfo()
{
	m_vecPackageCryptKeys.clear();
	m_mapPackageSDB.clear();
	if( m_pSerializedCryptKeyStream )
	{
		delete[] m_pSerializedCryptKeyStream;
		m_pSerializedCryptKeyStream = NULL;
	}
}

bool CClientPackageCryptInfo::LoadPackageCryptFile( const char* pCryptFile )
{
	FILE * fp = fopen(pCryptFile, "rb");

	if (!fp)
		return false;
	
	int iSDBDataOffset;
	fread(&iSDBDataOffset, sizeof(int), 1, fp);

	int iPackageCnt;
	fread( &iPackageCnt, sizeof(int), 1, fp );
	m_nCryptKeyPackageCnt += iPackageCnt;

	int iCryptKeySize = iSDBDataOffset - 2*sizeof(int);

	{

		if (0 == iCryptKeySize)
		{
			SPDLOG_WARN("[PackageCryptInfo] failed to load crypt key. (file: {}, key size: {})", pCryptFile, iCryptKeySize);
			m_nCryptKeyPackageCnt -= iPackageCnt;
		}
		else
		{
			int nCurKeySize = (int)m_vecPackageCryptKeys.size();
			m_vecPackageCryptKeys.resize( nCurKeySize + sizeof(int) + iCryptKeySize);

			memcpy( &m_vecPackageCryptKeys[nCurKeySize], &iCryptKeySize, sizeof(int));
			fread( &m_vecPackageCryptKeys[nCurKeySize + sizeof(int)], sizeof(BYTE), iCryptKeySize, fp );
			SPDLOG_WARN("[PackageCryptInfo] {} loaded. (key size: {}, count: {}, total: {})", pCryptFile, iCryptKeySize, iPackageCnt, m_nCryptKeyPackageCnt);
		}
	}

	//about SDB data
	//total packagecnt (4byte)
	//	for	packagecnt 
	//		db name hash 4byte( stl.h stringhash ) +child node size(4byte)

	//stream to client
	//		sdb file cnt( 4byte )
	//		for	sdb file cnt
	//			filename hash ( stl.h stringhash )
	//			related map name size(4), relate map name
	//			sdb block size( 1byte )
	//			sdb blocks 

	int iSDBPackageCnt;
	fread(&iSDBPackageCnt, sizeof(int), 1, fp);
	
	DWORD dwPackageNameHash, dwPackageStreamSize, dwSDBFileCnt, dwFileNameHash, dwMapNameSize;

	std::string	strRelatedMapName;

	if (0 == iCryptKeySize && 0 == iSDBPackageCnt)
		return false;

	for( int i = 0; i < iSDBPackageCnt; ++i )
	{
		fread(&dwPackageNameHash, sizeof(DWORD), 1, fp);
		fread(&dwPackageStreamSize, sizeof(DWORD), 1, fp);

		fread(&dwSDBFileCnt, sizeof(DWORD), 1, fp);

		SPDLOG_INFO("[PackageCryptInfo] SDB Loaded. (Name Hash : {}, Stream Size: {}, File Count: {})", dwPackageNameHash,dwPackageStreamSize, dwSDBFileCnt);

		for( int j = 0; j < (int)dwSDBFileCnt; ++j )
		{
			fread(&dwFileNameHash, sizeof(DWORD), 1, fp);
			fread(&dwMapNameSize, sizeof(DWORD), 1, fp);

			strRelatedMapName.resize( dwMapNameSize );
			fread(&strRelatedMapName[0], sizeof(BYTE), dwMapNameSize, fp);

			SPDLOG_INFO("[PackageCryptInfo] \t SDB each file info loaded.(MapName: {}, NameHash: {})", strRelatedMapName.c_str(), dwFileNameHash);

			BYTE bSDBStreamSize;
			std::vector<BYTE> vecSDBStream;
			fread(&bSDBStreamSize, sizeof(BYTE), 1, fp);

			vecSDBStream.resize(bSDBStreamSize);
			fread(&vecSDBStream[0], sizeof(BYTE), bSDBStreamSize, fp);

			//reconstruct it 
			TPackageSDBMap::iterator it = m_mapPackageSDB.find( strRelatedMapName );
			if( it == m_mapPackageSDB.end() )
			{
				TPerFileSDBInfo fileSDBInfo;
				m_mapPackageSDB[strRelatedMapName] = fileSDBInfo;	
			}

			TSupplementaryDataBlockInfo	SDBInfo;
			std::vector<TSupplementaryDataBlockInfo>& rSDBInfos = m_mapPackageSDB[strRelatedMapName].vecSDBInfos;
			{
				SDBInfo.dwPackageIdentifier = dwPackageNameHash;
				SDBInfo.dwFileIdentifier    = dwFileNameHash;
				SDBInfo.vecSDBStream.resize( bSDBStreamSize );

				memcpy(&SDBInfo.vecSDBStream[0], &vecSDBStream[0], bSDBStreamSize );

				rSDBInfos.push_back( SDBInfo );
			}
		}
	}

	fclose(fp);
	return  true;
}


bool CClientPackageCryptInfo::LoadPackageCryptInfo( const char* pCryptInfoDir )
{
	DIR * pDir = opendir(pCryptInfoDir);

	if (!pDir)
		return false;

	m_nCryptKeyPackageCnt = 0;
	if( m_pSerializedCryptKeyStream )
	{
		delete[] m_pSerializedCryptKeyStream;
		m_pSerializedCryptKeyStream = NULL;
	}

	m_mapPackageSDB.clear();
	m_vecPackageCryptKeys.clear();

	const char szPrefixCryptInfoFile[] = "cshybridcrypt";

	dirent * pDirEnt;
	while ((pDirEnt = readdir(pDir)))
	{
		//if (strncmp( &(pDirEnt->d_name[0]), szPrefixCryptInfoFile, strlen(szPrefixCryptInfoFile)) )
		if (std::string::npos == std::string(pDirEnt->d_name).find(szPrefixCryptInfoFile))
		{
			SPDLOG_DEBUG("[PackageCryptInfo] {} is not crypt file. pass!", pDirEnt->d_name);
			continue;
		}

		std::string strFullPathName = std::string(pCryptInfoDir) + std::string(pDirEnt->d_name);

		SPDLOG_DEBUG("[PackageCryptInfo] Try to load crypt file: {}", strFullPathName.c_str());

		if (false == LoadPackageCryptFile( strFullPathName.c_str() ))
			SPDLOG_DEBUG("[PackageCryptInfo] Failed to load {}", strFullPathName.c_str());
	}

	closedir(pDir);
	return true;
}

void CClientPackageCryptInfo::GetPackageCryptKeys( BYTE** ppData, int& iDataSize )
{
	int nCryptKeySize = m_vecPackageCryptKeys.size();
	int iStreamSize   = sizeof(int)+nCryptKeySize;

	//NOTE : Crypt Key Info isn`t updated during runtime. ( in case of file reloading all data is cleared & recreated )
	//it`s not safe but due to performance benefit we don`t do re-serialize.
	if( m_pSerializedCryptKeyStream )
	{
		*ppData   = m_pSerializedCryptKeyStream;
		iDataSize = iStreamSize;
		return;
	}
	
	if( nCryptKeySize > 0 )
	{
		m_pSerializedCryptKeyStream = new BYTE[iStreamSize];
		memcpy(&m_pSerializedCryptKeyStream[0], &m_nCryptKeyPackageCnt, sizeof(int) );
		memcpy(&m_pSerializedCryptKeyStream[sizeof(int)], &m_vecPackageCryptKeys[0], nCryptKeySize );

		*ppData   = m_pSerializedCryptKeyStream;
		iDataSize = iStreamSize;
	}
	else
	{
		*ppData = NULL;
		iDataSize = 0;
	}
}


bool CClientPackageCryptInfo::GetRelatedMapSDBStreams(const char* pMapName, BYTE** ppData, int& iDataSize )
{
	std::string strLowerMapName = pMapName;
	stl_lowers(strLowerMapName);

	TPackageSDBMap::iterator it = m_mapPackageSDB.find( strLowerMapName.c_str() );
	if( it == m_mapPackageSDB.end() || it->second.vecSDBInfos.size() == 0 )
	{
		SPDLOG_ERROR("GetRelatedMapSDBStreams Failed({})", strLowerMapName.c_str());
		return false;
	}

	*ppData = it->second.GetSerializedStream();
	iDataSize = it->second.GetSize();

	return true;
}

