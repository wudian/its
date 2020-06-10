#include "account/CTPMarginCommision.h"
#include <fstream>
#include <QtCore/QDate>
#include "common/Directory.h"
#include "datalib/SymbolInfoSet.h"
#include "common/Global.h"
#include "datalib/SymbolChanger.h"

using namespace std;

//namespace itstation {

#define DateFormat "yyyy-MM-dd"

CTPMarginCommision::CTPMarginCommision() 
	: api_(NULL) 
{
	api_ = MarketTradeApiFactory::CreateMarketTradeApi(MarketTradeApiFactory::kCtpFutureApi);
	info_set_ = SymbolInfoSet::Instance();
}

CTPMarginCommision::~CTPMarginCommision() {
	if (api_) {delete api_; api_ = NULL; }

}

void CTPMarginCommision::QryMargin(const std::string& symbol) {
	if (margins_[symbol].need_qry) {
		std::string err;
		api_->QryMargin(symbol, err);
		margins_[symbol].need_qry = false;
	}
}

void CTPMarginCommision::QryCommision(const std::string& symbol) {
	if (commisions_[symbol].need_qry) {
		std::string err;
		api_->QryCommision(symbol, err);
		commisions_[symbol].need_qry = false;
	}
}

PriceType CTPMarginCommision::CalcMargin(const Symbol &symbol, OrderDirection direction, PriceType open_price, VolumeType open_volume) {
	if (direction == LONG_DIRECTION)
	{
		return margins_[symbol.instrument].margin.LongMarginRatioByMoney * info_set_->GetVolMulti(symbol) * open_price * open_volume
			+ margins_[symbol.instrument].margin.LongMarginRatioByVolume * info_set_->GetVolMulti(symbol) * open_volume;
	}
	else
	{
		return margins_[symbol.instrument].margin.ShortMarginRatioByMoney * info_set_->GetVolMulti(symbol) * open_price * open_volume
			+ margins_[symbol.instrument].margin.ShortMarginRatioByVolume * info_set_->GetVolMulti(symbol) * open_volume;
	}
}

PriceType CTPMarginCommision::CalcCommision(const Symbol &symbol, OpenCloseFlag open_close_flag, PriceType open_price, VolumeType open_volume) {
	CommisionInfo &commision = commisions_[GetFutureProName(symbol.instrument)].commision;
	if (OPEN_ORDER == open_close_flag) {
		return open_price * open_volume * commision.OpenRatioByMoney
			+ open_volume * commision.OpenRatioByVolume;
	}
	else if (CLOSE_TODAY_ORDER == open_close_flag) {
		return open_price * open_volume * commision.CloseTodayRatioByMoney
			+ open_volume * commision.CloseTodayRatioByVolume;
	}
	else {
		return open_price * open_volume * commision.CloseRatioByMoney
			+ open_volume * commision.CloseRatioByVolume;
	}
}


void CTPMarginCommision::Init() {
	api_->SetSecurityInfoSpi(this);

	info_set_ = SymbolInfoSet::Instance();
	/*for (std::vector<Symbol>::const_iterator iter = info_set_->FutureSymbols().begin(); iter != info_set_->FutureSymbols().end(); ++iter) {
		margins_[iter->instrument] = LocalMargin();
		commisions_[GetFutureProName(iter->instrument)] = LocalCommision();
	}

	std::string its_home = Global::Instance()->GetItsHome();
	margin_path_ =  Global::Instance()->GetItsHome() + "/data/FutureMargin.txt";	
	if (Directory::IsDirExist(margin_path_)) {
		ifstream margin_file(margin_path_.c_str());
		while (!margin_file.eof()) {
			char buf[128] = {0};
			margin_file.getline(buf, 128);
			if (strlen(buf) == 0) break;
			LocalMargin local_margin;
			std::string date = local_margin.margin.FromStr(buf);
			if (QDate::currentDate() == QDate::fromString(date.c_str(), DateFormat)) local_margin.need_qry = false;
			margins_[local_margin.margin.instrument] = local_margin;
		}
		margin_file.close();		
	}
	commison_path_ = its_home + "/data/FutureCommision.txt";
	if (Directory::IsDirExist(commison_path_)) {
		ifstream commision_file(commison_path_.c_str());
		while (!commision_file.eof()) {
			char buf[128] = {0};
			commision_file.getline(buf, 128);
			if (strlen(buf) == 0) break;
			LocalCommision local_commision;
			std::string date = local_commision.commision.FromStr(buf);
			if (QDate::currentDate() == QDate::fromString(date.c_str(), DateFormat)) local_commision.need_qry = false;
			commisions_[local_commision.commision.instrument] = local_commision;
		}
		commision_file.close();
	}*/
}

void CTPMarginCommision::Denit() {
	/*QString date = QDate::currentDate().toString(DateFormat);
	date += ",";
	for (std::map<std::string, LocalMargin>::const_iterator iter = margins_.begin(); iter != margins_.end(); ++iter) {
		if (!strcmp(iter->second.margin.instrument, "")) continue;
		fwrite(date.toLocal8Bit().constData(), strlen(date.toLocal8Bit().constData()), 1,margin_fp_);
		fwrite(iter->second.margin.Str().c_str(), iter->second.margin.Str().size(), 1, margin_fp_);
		fwrite("\n", 1, 1, margin_fp_);
	}

	for (std::map<std::string, LocalCommision>::const_iterator iter = commisions_.begin(); iter != commisions_.end(); ++iter) {
		if (!strcmp(iter->second.commision.instrument, "")) continue;
		fwrite(date.toLocal8Bit().constData(), strlen(date.toLocal8Bit().constData()), 1, commision_fp_);
		fwrite(iter->second.commision.Str().c_str(), iter->second.commision.Str().size(), 1, commision_fp_);
		fwrite("\n", 1, 1, commision_fp_);
	}*/

	info_set_->Denit();
}


void CTPMarginCommision::OnMarginInfo(const MarginInfo& info, bool is_last) {	
	margins_[info.instrument].margin = info;
}

void CTPMarginCommision::OnCommisionInfo(const CommisionInfo& info, bool is_last) {	
	commisions_[GetFutureProName(info.instrument)].commision = info;
}

