#include "stdafx.h"
#include "strEx.h"
#include "NRPESocket.h"

/**
 * Default c-tor
 */
NRPESocket::NRPESocket() {
}

NRPESocket::~NRPESocket() {
}



typedef short int16_t;
typedef unsigned long u_int32_t;

static unsigned long crc32_table[256];
static bool hascrc32 = false;
void generate_crc32_table(void){
	unsigned long crc, poly;
	int i, j;
	poly=0xEDB88320L;
	for(i=0;i<256;i++){
		crc=i;
		for(j=8;j>0;j--){
			if(crc & 1)
				crc=(crc>>1)^poly;
			else
				crc>>=1;
		}
		crc32_table[i]=crc;
	}
	hascrc32 = true;
}
unsigned long calculate_crc32(const char *buffer, int buffer_size){
	if (!hascrc32)
		generate_crc32_table();
	register unsigned long crc;
	int this_char;
	int current_index;

	crc=0xFFFFFFFF;

	for(current_index=0;current_index<buffer_size;current_index++){
		this_char=(int)buffer[current_index];
		crc=((crc>>8) & 0x00FFFFFF) ^ crc32_table[(crc ^ this_char) & 0xFF];
	}

	return (crc ^ 0xFFFFFFFF);
}


class NRPEPacket {
public:
	static const short queryPacket = 1;
	static const short responsePacket = 2;
	static const short version2 = 2;
private:
	typedef struct packet {
		int16_t   packet_version;
		int16_t   packet_type;
		u_int32_t crc32_value;
		int16_t   result_code;
		char      buffer[1024];
	} packet;
	std::string payload_;
	short type_;
	short version_;
	NSCAPI::nagiosReturn result_;
	unsigned int crc32_;
	unsigned int calculatedCRC32_;
	char *tmpBuffer;
public:
	NRPEPacket(const char *buffer) : tmpBuffer(NULL) {
		const packet *p = reinterpret_cast<const packet*>(buffer);
		type_ = ntohs(p->packet_type);
		assert( (type_ == queryPacket)||(type_ == responsePacket));
		version_ = ntohs(p->packet_version);
		assert(version_ == version2);
		crc32_ = ntohl(p->crc32_value);
		// Verify CRC32
		// @todo Fix this, currently we need a const buffer so we cannot change the crc to 0.
		char * tb = new char[getBufferLength()];
		memcpy(tb, buffer, getBufferLength());
		packet *p2 = reinterpret_cast<packet*>(tb);
		p2->crc32_value = 0;
		calculatedCRC32_ = calculate_crc32(tb, getBufferLength());
		delete [] tb;
		// Verify CRC32 end
		result_ = NSCHelper::int2nagios(ntohs(p->result_code));
		payload_ = std::string(p->buffer);
	}
	NRPEPacket(short type, short version, NSCAPI::nagiosReturn result, std::string payLoad) 
		: tmpBuffer(NULL) 
		,type_(type)
		,version_(version)
		,result_(result)
		,payload_(payLoad)
	{
	}
	~NRPEPacket() {
		delete [] tmpBuffer;
	}
	unsigned short getVersion() const { return version_; }
	unsigned short getType() const { return type_; }
	unsigned short getResult() const { return result_; }
	std::string getPayload() const { return payload_; }
	const char* getBuffer() {
		delete [] tmpBuffer;
		tmpBuffer = new char[getBufferLength()];
		packet *p = reinterpret_cast<packet*>(tmpBuffer);
		p->result_code = htons(NSCHelper::nagios2int(result_));
		p->packet_type = htons(type_);
		p->packet_version = htons(version_);
		p->crc32_value = 0;
		strncpy(p->buffer, payload_.c_str(), 1023);
		p->buffer[1024] = 0;
		p->crc32_value = htonl(calculate_crc32(tmpBuffer, getBufferLength()));
		return tmpBuffer;
	}
	bool verifyCRC() {
		return calculatedCRC32_ == crc32_;
	}
	const unsigned int getBufferLength() const {
		return sizeof(packet);
	}
};

void NRPESocket::onAccept(simpleSocket::Socket client) {
	if (!inAllowedHosts(client.getAddrString())) {
		NSC_LOG_ERROR("Unothorized access from: " + client.getAddrString());
		client.close();
		return;
	}
	simpleSocket::DataBuffer block;
	client.readAll(block);
	NRPEPacket p(block.getBuffer());
	if (p.getType() != NRPEPacket::queryPacket) {
		NSC_LOG_ERROR("Request is not a query.");
		client.close();
		return;
	}
	if (p.getVersion() != NRPEPacket::version2) {
		NSC_LOG_ERROR("Request had unsupported version.");
		client.close();
		return;
	}
	if (!p.verifyCRC()) {
		NSC_LOG_ERROR("Request had invalid checksum.");
		client.close();
		return;
	}
	strEx::token cmd = strEx::getToken(p.getPayload(), '!');
	std::string msg, perf;
	NSC_DEBUG_MSG_STD("Command: " + cmd.first);
	NSC_DEBUG_MSG_STD("Arguments: " + cmd.second);

	if (NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_ALLOW_ARGUMENTS, 0) == 0) {
		if (!cmd.second.empty()) {
			NSC_LOG_ERROR("Request contained arguments (not currently allowed).");
			client.close();
			return;
		}
	}
	if (NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_ALLOW_NASTY_META, 0) == 0) {
		if (cmd.first.find_first_of(NASTY_METACHARS) != std::string::npos) {
			NSC_LOG_ERROR("Request command contained illegal metachars!");
			client.close();
			return;
		}
		if (cmd.second.find_first_of(NASTY_METACHARS) != std::string::npos) {
			NSC_LOG_ERROR("Request arguments contained illegal metachars!");
			client.close();
			return;
		}
	}

	NSCAPI::nagiosReturn ret = NSCModuleHelper::InjectSplitAndCommand(cmd.first, cmd.second, '!', msg, perf);
	if (perf.empty()) {
		NRPEPacket p2(NRPEPacket::responsePacket, NRPEPacket::version2, ret, msg);
		client.send(p2.getBuffer(), p2.getBufferLength(), 0);
	} else {
		NRPEPacket p2(NRPEPacket::responsePacket, NRPEPacket::version2, ret, msg + "|" + perf);
		client.send(p2.getBuffer(), p2.getBufferLength(), 0);
	}
	client.close();
}

