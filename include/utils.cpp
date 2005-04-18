#include <utils.h>


checkHolders::Size::Size() : type_(none), value_(0) {}

void checkHolders::Size::set(std::string s) {
	std::string::size_type p = s.find_first_of('%');
	if (p == std::string::npos) {
		value_ = strEx::stoi64_as_BKMG(s);
		type_ = size;
	} else {
		value_ = strEx::stoi64(s.substr(0,p));
		type_ = percentage;
	}
}
checkHolders::Size::checkResult checkHolders::Size::check(long long value, long long max) const {
	long long p = getValue(value, max);
	if (p == value_)
		return same;
	else if (p > value_)
		return above;
	return below;
}
std::string checkHolders::Size::prettyPrint(std::string name, long long value, long long max) const {
	if (type_ == percentage)
		return name + ": " + strEx::itos(getValue(value, max));
	return name + ": " + strEx::itos_as_BKMG(getValue(value, max));
}

std::string checkHolders::SizeMaxMin::printPerfHead(bool percentage, std::string name, long long value, long long total) 
{
	if (percentage)
		return name + "=" + strEx::itos(value*100/total)+ "% ";
	return name + "=" + strEx::itos(value) + ";";
}
std::string checkHolders::SizeMaxMin::printPerfData(bool percentage, long long value, long long total)
{
	if (percentage) {
		if (max.hasBounds()) {
			if (max.isPercentage()) {
				return strEx::itos(max.value_) + ";";
			} else {
				return strEx::itos((max.value_*total)/100) + ";";
			}
		} else if (min.hasBounds()) {
			if (min.isPercentage()) {
				return strEx::itos(min.value_) + ";";
			} else {
				return strEx::itos((min.value_*total)/100) + ";";
			}
		}
	}
	return "0;";
}

std::string checkHolders::SizeMaxMin::printPerf(std::string name, long long value, long long total, SizeMaxMin &warn, SizeMaxMin &crit)
{
	std::string s;
	bool bPercentage = warn.isPercentage() && crit.isPercentage();
	s = printPerfHead(bPercentage, name, value, total);
	s += warn.printPerfData(bPercentage, value, total);
	s += crit.printPerfData(bPercentage, value, total);
	s += " ";
	return s;
}

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
