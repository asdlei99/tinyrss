#include "stdpch.h"

#include "zlib.h"
#pragma comment(lib,"zdll.lib")

#include "http.h"

CMyHttp::CMyHttp(const char* host,const char* port)
{
	m_port = port;
	if(*host>='0' && *host<='9'){
		m_ip = host;
	}else{
		PHOSTENT pHost = nullptr;
		pHost = ::gethostbyname(host);
		if(pHost){
			char str[16];
			unsigned char* pip = (unsigned char*)*pHost->h_addr_list;
			sprintf(str,"%d.%d.%d.%d",pip[0],pip[1],pip[2],pip[3]);
			m_ip = str;
		}
		else{
			throw "�޷�������������IP��ַ!";
		}
	}
}

bool CMyHttp::Connect()
{
	int rv;
	SOCKADDR_IN addr;

	SOCKET s = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(s==INVALID_SOCKET)
		throw "�����׽���ʧ��!";

	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(m_port.c_str()));
	addr.sin_addr.s_addr = inet_addr(m_ip.c_str());
	rv = connect(s,(sockaddr*)&addr,sizeof(addr));
	if(rv == SOCKET_ERROR)
		throw "�޷����ӵ�������!";
	m_sock = s;

	return true;
}

bool CMyHttp::Disconnect()
{
	closesocket(m_sock);
	return true;
}

bool CMyHttp::PostData(const char* data,size_t sz, int /*timeouts*/,CMyHttpResonse** ppr)
{
	CMyHttpResonse* R = new CMyHttpResonse;
	if(!send(data,sz)){
		throw "δ�ܳɹ����������������!";
	}
	parseResponse(R);
	*ppr = R;
	return true;
}

bool CMyHttp::parseResponse(CMyHttpResonse* R)
{
	const char* kHttpError = "HTTP���Ľ�����������, �������߷�����Ϣ!";
	char c;
	//��HTTP�汾
	while(recv(&c,1) && c!=' '){
		R->version += c;
	}
	//��״̬��
	while(recv(&c,1) && c!= ' '){
		R->state += c;
	}
	//��״̬˵��
	while(recv(&c,1) && c!='\r'){
		R->comment += c;
	}
	// \r�Ѷ�,����һ��\n
	recv(&c,1);
	if(c!='\n')
		throw kHttpError;
	//����ֵ��
	for(;;){
		std::string k,v;
		//����
		recv(&c,1);
		if(c=='\r'){
			recv(&c,1);
			assert(c=='\n');
			if(c!='\n')
				throw kHttpError;
			break;
		}
		else
			k += c;
		while(recv(&c,1) && c!=':'){
			k += c;
		}
		//��ֵ
		recv(&c,1);
		assert(c==' ');
		if(c!=' ')
			throw kHttpError;
		while(recv(&c,1) && c!='\r'){
			v += c;
		}
		// ���� \n
		recv(&c,1);
		assert(c=='\n');
		if(c!='\n')
			throw kHttpError;
		//���
		R->heads[k] = v;
	}
	//��������ݷ��صĻ�, ���ݴ����￪ʼ

	if(R->state != "200"){
		static std::string t; //Oops!!!
		t = R->version;
		t += " ";
		t += R->state;
		t += " ";
		t += R->comment;
		throw t.c_str();
	}
	
	bool bContentLength,bTransferEncoding;
	bContentLength = R->heads.count("Content-Length")!=0;
	bTransferEncoding = R->heads.count("Transfer-Encoding")!=0;

	if(bContentLength){
		int len = atoi(R->heads["Content-Length"].c_str());
		char* data = new char[len+1]; //�����1�ֽ�,ʹ���Ϊ'\0',������ʵ�ʳ���
		memset(data,0,len+1);		//�����Բ�Ҫ
		recv(data,len);

		bool bGzipped = R->heads["Content-Encoding"] == "gzip";
		if(bGzipped){
			CMyByteArray gzip;
			ungzip(data, len, &gzip);
			gzip.Duplicate(&R->data, true);
			R->size = gzip.GetSize();
			return true;
		}

		R->data = data;
		R->size = len;	//û�ж����1�ֽ�

		return true;
	}
	if(bTransferEncoding){
		//�ο�:http://www.w3.org/Protocols/rfc2616/rfc2616-sec3.html#sec3.6.1
		if(R->heads["Transfer-Encoding"] != "chunked")
			throw "δ����Ĵ��ͱ�������!";

		auto char_val = [](char ch)
		{
			int val = 0;
			if(ch>='0' && ch<='9') val = ch-'0'+ 0;
			if(ch>='a' && ch<='f') val = ch-'a'+10;
			if(ch>='A' && ch<='F') val = ch-'A'+10;
			return val;
		};

		int len;
		CMyByteArray bytes;

		for(;;){
			len = 0;
			//��ȡchunked���ݳ���
			while(recv(&c,1) && c!=' ' && c!='\r'){
				len = len*16 + char_val(c);
			}

			//������ǰchunked�鳤�Ȳ���
			if(c!='\r'){
				while(recv(&c,1) && c!='\r')
					;
			}
			recv(&c,1);
			assert(c=='\n');
			if(c!='\n')
				throw kHttpError;
			//�鳤��Ϊ��,chunked���ݽ���
			if(len == 0){
				break;
			}

			//���¿�ʼ��ȡchunked���ݿ����ݲ��� + \r\n
			char* p = new char[len+2];
			recv(p,len+2);

			assert(p[len+0] == '\r');
			assert(p[len+1] == '\n');
			if(p[len+0]!='\r')
				throw kHttpError;
			if(p[len+1]!='\n')
				throw kHttpError;

			bytes.Write(p,len);
			delete[] p;
		}
		//chunked ���ݶ�ȡ����
		recv(&c,1);
		assert(c == '\r');
		if(c!='\r')
			throw kHttpError;
		recv(&c,1);
		assert(c == '\n');
		if(c!='\n')
			throw kHttpError;

		bool bGzipped = R->heads["Content-Encoding"] == "gzip";
		if(bGzipped){
			CMyByteArray gzip;
			if(!ungzip(bytes.GetData(), bytes.GetSize(), &gzip))
				throw "HTTP����Gzip��ѹʱ��������! �뷴��!";
			gzip.Duplicate(&R->data, true);
			R->size = gzip.GetSize();
			return true;
		}

		bytes.Duplicate(&R->data, true);
		R->size = bytes.GetSize();
		return true;
	}
	//�����������, ˵��û����Ϣ����
	R->data = 0;
	R->size = 0;
	return true;
}

bool CMyHttp::send(const void* data,size_t sz)
{
	const char* p = (const char*)data;
	int sent = 0;
	while(sent < (int)sz){
		int cb = ::send(m_sock,p+sent,sz-sent,0);
		if(cb==0) return false;
		sent += cb;
	}
	return true;
}

int CMyHttp::recv(char* buf,size_t sz)
{
	int rv;
	size_t read=0;
	while(read != sz){
		rv = ::recv(m_sock,buf+read,sz-read,0);
		if(!rv || rv == -1){
			throw "recv() �������!";
		}
		read += rv;
	}
	return read; // equals to sz
}

bool CMyHttp::ungzip(const void* data, size_t sz, CMyByteArray* bytes)
{
	int ret;
	z_stream strm;
	const int buf_size = 16384; //16KB
	unsigned char out[buf_size];

	strm.zalloc   = Z_NULL;
	strm.zfree    = Z_NULL;
	strm.opaque   = Z_NULL;
	strm.avail_in = 0;
	strm.next_in  = Z_NULL;

	ret = inflateInit2(&strm, MAX_WBITS+16);
	if(ret != Z_OK)
		return false;

	strm.avail_in = sz;
	strm.next_in = (Bytef*)data;
	do{
		unsigned int have;
		strm.avail_out = buf_size;
		strm.next_out = out;
		ret = inflate(&strm, Z_NO_FLUSH);
		if(ret!=Z_OK && ret!=Z_STREAM_END)
			break;
		
		have = buf_size-strm.avail_out;
		if(have){
			bytes->Write(out, have);
		}
		if(ret == Z_STREAM_END)
			break;
	}while(strm.avail_out == 0);
	inflateEnd(&strm);
	return ret==Z_STREAM_END;
}

//////////////////////////////////////////////////////////////////////////

CMyByteArray::CMyByteArray()
	: m_pData(0)
	, m_iDataSize(0)
	, m_iDataPos(0)
{

}

CMyByteArray::~CMyByteArray()
{
	if(m_pData){
		delete[] m_pData;
		m_pData = 0;
	}
}

void* CMyByteArray::GetData()
{
	return m_pData;
}

size_t CMyByteArray::GetSize()
{
	return GetSpaceUsed();
}

size_t CMyByteArray::Write( const void* data,size_t sz )
{
	if(sz > GetSpaceLeft()) ReallocateDataSpace(sz);
	memcpy((char*)m_pData+GetSpaceUsed(),data,sz);
	GetSpaceUsed() += sz;
	return sz;
}

bool CMyByteArray::MakeNull()
{
	int x = 0;
	Write(&x, sizeof(x));
	GetSpaceUsed() -= sizeof(x);
	return true;
}

bool CMyByteArray::Duplicate( void** ppData, bool bMakeNull /*= false*/ )
{
	int addition = bMakeNull ? sizeof(int) : 0;
	if(bMakeNull) MakeNull();
	char* p = new char[GetSpaceUsed()+addition];
	memcpy(p,GetData(),GetSize()+addition);
	*ppData = p;
	return true;
}

size_t CMyByteArray::GetGranularity()
{
	return (size_t)1<<20;
}

size_t CMyByteArray::GetSpaceLeft()
{
	//���m_iDataPos == m_iDataSize
	//�����ȷ, ��ò�Ʊ��ʽ����?
	// -1 + 1 = 0, size_t �ò���-1, 0x~FF + 1,Ҳ��0
	return m_iDataSize-1 - m_iDataPos + 1;
}

size_t& CMyByteArray::GetSpaceUsed()
{
	//����-��ʼ+1
	//return m_iDataPos-1 -0 + 1;
	return m_iDataPos;
}

void CMyByteArray::ReallocateDataSpace( size_t szAddition )
{
	//ֻ�д������Ż����ռ�,Ҳ�����ô˺���ǰ������ռ�ʣ��
	//assert(GetSpaceLeft()<szAddition);

	//�����ȥʣ��ռ��Ӧ���ӵĿռ��С
	size_t left = szAddition - GetSpaceLeft();

	//���������ȼ��������ʣ��
	size_t nBlocks = left / GetGranularity();
	size_t cbRemain = left - nBlocks*GetGranularity();
	if(cbRemain) ++nBlocks;

	//�����¿ռ䲢���·���
	size_t newDataSize = m_iDataSize + nBlocks*GetGranularity();
	void*  pData = (void*)new char[newDataSize];

	//����ԭ�������ݵ��¿ռ�
	//memcpy֧�ֵ�3��������0��?
	if(GetSpaceUsed()) memcpy(pData,m_pData,GetSpaceUsed());

	if(m_pData) delete[] m_pData;
	m_pData = pData;
	m_iDataSize = newDataSize;
}
