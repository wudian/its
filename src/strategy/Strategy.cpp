
#include <iostream>
#include "strategy/Strategy.h"
//#include "dataserver/SubBarsApi.h"
#include "common/Directory.h"
#include "common/XmlConfig.h"
//#include "dataserver/RuntimeDataApiManager.h"
#include "datalib/SymbolInfoSet.h"
#include "datalib/SimpleMath.h"

Strategy::Strategy()
{
	strncpy(user_tag_, "002907", sizeof(user_tag_));
}

Strategy::~Strategy()
{
}

bool Strategy::Init(string &err){
	if (!SymbolInfoSet::GetInstance()->Init(err)) return false;

	// 读配置参数.
	XmlConfig config(Global::GetInstance()->GetConfigDir() + "config.xml");
	if (!config.Load()) 
	{
		err = "load config error";
		return false;
	}
	XmlNode node = config.FindNode("Strategy_MA");		
	std::string code = node.GetValue("code");
	symbol_ =  Symbol(PRODUCT_FUTURE, EXCHANGE_DCE, code.c_str());
	std::string period = node.GetValue("kline_period");
	s_ma_.Period = atoi(node.GetValue("short_ma").c_str());
	l_ma_.Period = atoi(node.GetValue("long_ma").c_str());
	state_ = 0;
	int jump_nums = atoi(node.GetValue("jump_nums").c_str());
	price_offset_ = SymbolInfoSet::GetInstance()->GetPriceTick(symbol_) * jump_nums;
	submit_hands_ = atoi(node.GetValue("submit_hands").c_str());
	cancel_interval_ = node.GetValue("cancel_interval");
	target_profit_value_ = atof(node.GetValue("target_profit_value").c_str());
	
	
	// 行情.
	RuntimeDataApiManager* run_api_manager = new RuntimeDataApiManager(SubBarsApi::GetInstance());
	SubBarsApi::GetInstance()->SetRunApiManager(run_api_manager); //设置实时行情接口		
	if (!run_api_manager->CreateApi(Global::GetInstance()->GetAppConfigPath(), err))	return false;

	DateTime now(NULL);
	run_api_manager->InitApi(RuntimeDataApiManager::ALL_API, now.time.hour > 18 || now.time.hour < 5);
	if (!SubBarsApi::GetInstance()->Init(Global::GetInstance()->GetAppConfigPath(), err)) 	return false;

	
	if (!SubBars(symbol_, period.c_str())) return false;

	// 交易.
	TradeEngine::Instance()->SetSpi(this, this);	
	if (!TradeEngine::Instance()->Init(Global::GetInstance()->GetAppConfigPath(), err))return false;
	
	return true;
}
/*
bool Strategy::SubBars(const Symbol& symbol, const std::string& period)
{
	HisDataParam info;
	info.symbol = symbol;
	info.count_mode = COUNT_MOD_TIME_RANGE;
	info.dim_count = atoi(period.substr(0, period.size() - 1).c_str());
	if (info.dim_count <= 0 || info.dim_count > 500)
	{
		std::cout << "订阅行情失败，无效周期:" << period;
		return false;
	}
	switch (period.at(period.size() - 1))
	{
	case 'm':case 'M':
		info.dim = DIMENSION_MINUTE;
		break;
	case 'h':case 'H':
		info.dim = DIMENSION_HOUR;
		break;
	case 'd':case 'D':
		info.dim = DIMENSION_DAY;
		break;
	default:
		std::cout << "订阅行情失败，无效周期:" << period;
		return false;
	}

	DateTime now(NULL);
	info.mode_data = now.date.year * 10000LL + now.date.month * 100LL + now.date.day;
	info.mode_data *= 100000000LL;
	if (!SubBarsApi::GetInstance()->SubBars(info, this))
	{
		cout << "SubBars error: " << SubBarsApi::GetInstance()->GetLastError() << endl;
		return false;
	}
	return true;
}

// OnTick
void Strategy::OnUpdateKline(const Bars *_bars, bool is_new){
	Bars *bars = (Bars*)_bars;

	//cout << "OnTick: " << bars->symbol.instrument << "  "  << bars->DimCount << "  " << bars->UpdateTime.Str() << "  " << bars->LastPrice << endl;

	double pre_price = TradeEngine::Instance()->GetPosiPrePrice(bars->symbol);
	TradeEngine::Instance()->SetPosiLastPrice(bars->symbol, bars->LastPrice);

	Locker lock(&pos_mutex_);
	for (int i=0; i < positions_.size(); ++i){
		if (positions_[i].symbol == bars->symbol){
			if (PriceUnEqual(pre_price, bars->LastPrice)) {
				PriceType pre_profit = float_profit_[i];
				if (PriceUnEqual(pre_profit, 0.)) {
					float_profit_[i] = TradeEngine::Instance()->CalcFloatProfit(
						positions_[i].symbol, positions_[i].direction,
						positions_[i].open_price, bars->LastPrice, positions_[i].open_volume);
					PriceType diff_profit = float_profit_[i] - pre_profit;
					TradeEngine::Instance()->UpdateAccountProfit(diff_profit);		
				}
				else {
					TradeEngine::Instance()->UpdateAccountProfit(float_profit_[i], true);	
				}
			}
		}
	}
	
	std::vector<OrderData> orders;
	TradeEngine::Instance()->GetValidOrder(orders);
	for (int i=0; i < orders.size(); ++i)
	{
		DateTime time = orders[i].submit_time;	
		int num = atoi(cancel_interval_.substr(0, cancel_interval_.size() - 1).c_str());
		if (cancel_interval_[1]=='m')
			time.AddMin(num);
		else if (cancel_interval_[1]=='s')
			time.AddSec(num);
		if (time < bars->UpdateTime) //说明订单超过1分钟未成交
		{
			TradeEngine::Instance()->CancelOrder(orders[i]);
			OrderParamData param;
			param.symbol = orders[i].symbol;
			param.limit_price = orders[i].direction == LONG_DIRECTION ? bars->AskPrice[0] + price_offset_ : bars->BidPrice[0] - price_offset_;
			param.volume = orders[i].total_volume - orders[i].trade_volume;
			param.order_price_flag = LIMIT_PRICE_ORDER;
			strncpy(param.user_tag, user_tag_, sizeof(UserStrategyIdType));
			param.direction = orders[i].direction;
			param.open_close_flag = orders[i].open_close_flag;
			param.hedge_flag = HF_SPECULATION;
			TradeEngine::Instance()->SubmitOrder(param);
		}
	}

}
// OnBar
void Strategy::OnKlineFinish(const Bars *_bars){
	Bars *bars = (Bars*)_bars;

	s_ma_.Update(bars);
	l_ma_.Update(bars);
	cout << "s_ma_.Value[1]:"<<s_ma_.Value[1]<<" l_ma_.Value[1]:"<<l_ma_.Value[1]
	<<" s_ma_.Value[0]"<<s_ma_.Value[0]<<" l_ma_.Value[0]"<<l_ma_.Value[0]<<endl;
	switch (state_)
	{
	case 0:
		if (s_ma_.Value[1] < l_ma_.Value[1] && s_ma_.Value[0] > l_ma_.Value[0])
		{
			state_ = 1;
			TradeEngine::Instance()->Buy(bars->symbol, bars->AskPrice[0] + price_offset_, submit_hands_);
			cout << "buy open\n";
		}
		else if (s_ma_.Value[1] > l_ma_.Value[1] && s_ma_.Value[0] < l_ma_.Value[0])
		{
			state_ = -1;
			TradeEngine::Instance()->SellShort(bars->symbol, bars->BidPrice[0] - price_offset_, submit_hands_);
			cout << "sell open\n";
		}
		break;
	case 1:
		{
			if (s_ma_.Value[1] > l_ma_.Value[1] && s_ma_.Value[0] < l_ma_.Value[0])
			{
				PositionData long_pos;
				TradeEngine::Instance()->GetLongPositionBySymbol(long_pos, bars->symbol);

				state_ = 0;
				TradeEngine::Instance()->Sell(bars->symbol, bars->BidPrice[0] - price_offset_, long_pos.enable_today_volume);
				cout << "sell close\n";
			}
		}
		break;

	case -1:
		{
			if (s_ma_.Value[1] < l_ma_.Value[1] && s_ma_.Value[0] > l_ma_.Value[0])
			{
				PositionData short_pos;
				TradeEngine::Instance()->GetShortPositionBySymbol(short_pos, bars->symbol);

				state_ = 0;
				TradeEngine::Instance()->BuyCover(bars->symbol, bars->AskPrice[0] + price_offset_, short_pos.enable_today_volume);
				cout << "buy close\n";
			}
		}
	}

	TradeEngine::Instance()->QryCtpAccount();
	cout << "OnBar:  " << bars->End[0].Str() << "  O:" << bars->Open[0] << "  H:" << bars->High[0] << "  L:" << bars->Low[0]
	<< "  C:" <<bars->Close[0] << "  V:" << bars->Volume[0] << endl;
}

// Init: OnTick & OnBar
void Strategy::OnInitKline(const Bars *_bars){
	Bars *bars = (Bars*)_bars;
	// OnTick
	s_ma_.Update(bars);
	l_ma_.Update(bars);

	TradeEngine::Instance()->SetPosiLastPrice(bars->symbol, bars->LastPrice);

	// OnBar	



	cout << "Init OnTick: " << bars->symbol.instrument << "  "  << bars->DimCount << "  " << bars->UpdateTime.Str() << "  " << bars->LastPrice << endl;
	cout << "Init OnBar:  " << bars->End[0].Str() << "  O:" << bars->Open[0] << "  H:" << bars->High[0] << "  L:" << bars->Low[0]
	<< "  C:" <<bars->Close[0] << "  V:" << bars->Volume[0] << endl;
}
*/

void Strategy::UpdateAccount(){
	AccountData account;
	TradeEngine::Instance()->GetAccount(account);
	cout << "资金账号：\n"
		<< "静态权益"<<account.static_balance<<" 总盈亏"<<account.close_profit<<" 手续费"<<account.commision
		<< " 动态权益"<<account.asset_balance << " 占用保证金"<<account.margin_balance 
		<< " 下单冻结"<<account.frozen_balance << " 可用资金"<<account.enable_balance<< endl;

	// 账户资金到某个设定金额，自动全部平仓： 资金止盈，总的资金量达到设定值.
	if (account.asset_balance > target_profit_value_){
		PositionData long_pos, short_pos;
		double last_price = TradeEngine::Instance()->GetPosiPrePrice(symbol_);
		if (TradeEngine::Instance()->GetLongPositionBySymbol(long_pos, symbol_)){
			TradeEngine::Instance()->Sell(symbol_, last_price-price_offset_, long_pos.enable_today_volume+long_pos.enable_yestd_volume);
		}
		if (TradeEngine::Instance()->GetShortPositionBySymbol(short_pos, symbol_)){
			TradeEngine::Instance()->BuyCover(symbol_, last_price+price_offset_, short_pos.enable_today_volume+short_pos.enable_yestd_volume);
		}
	}
}

std::string OpenCloseStr(OpenCloseFlag flag)
{
	switch (flag)
	{
	case OPEN_ORDER:
		return ("开仓");
	case CLOSE_ORDER:
		return ("平仓");
	case CLOSE_TODAY_ORDER:
		return ("平今");
	case CLOSE_YESTERDAY_ORDER:
		return ("平昨");
	default:
		return "";
	}
}

std::string OrderStatusStr(OrderStatus status)
{
	switch(status)
	{
	case '0': return "未知";
	case '1': return "待报";
	case '2': return "未成交";
	case '3': return "部分成交";
	case '4': return "全部成交";
	case '5': return "待撤";
	case '6': return "已撤单";
	case '7': return "尚未触发";
	case '8': return "已触发";
	default: return "订单无效"; 
	}
}

void Strategy::UpdateValidOrders(){
	vector<OrderData> validOrders;
	TradeEngine::Instance()->GetValidOrder(validOrders);
	cout<<"有效单列表：\n";
	for (int i=0;i<validOrders.size();++i){
		cout<< ("委托编号:") << validOrders[i].order_id
			<< (" 合约:") << validOrders[i].symbol.instrument
			<< (" 买卖:") << (validOrders[i].direction == LONG_DIRECTION ? "买入" : "卖空")
			<< (" 开平:") << OpenCloseStr(validOrders[i].open_close_flag)
			<< (" 委托价格:") << validOrders[i].limit_price
			<< (" 委托数量:") << validOrders[i].total_volume
			<< (" 成交数量:") << validOrders[i].trade_volume
			<< (" 委托时间:") << validOrders[i].submit_time.time.Str()
			<< (" 状态:") << OrderStatusStr(validOrders[i].status)
			<< (" 状态信息:") << validOrders[i].status_msg<<endl;
	}
	cout<<"\n";
}
void Strategy::UpdateTrades(){
	vector<TradeData> trades;
	TradeEngine::Instance()->GetAllTrade(trades);
	cout<<"成交单列表：\n";
	for (int i=0;i<trades.size();++i){
		cout<< ("合约") << trades[i].symbol.instrument
			<< (" 委托编号:") << trades[i].order_id
			<< (" 成交编号:") << trades[i].trade_id
			<< (" 买卖:") << (trades[i].direction == LONG_DIRECTION ? "买入" : "卖空")
			<< (" 开平:") << OpenCloseStr(trades[i].open_close_flag)
			<< (" 成交价:") << trades[i].trade_price
			<< (" 数量:") << trades[i].trade_volume
			<< (" 成交时间:") << trades[i].trade_time.time.Str()<<endl;
	}
	cout<<"\n";
}
void Strategy::UpdatePositions(){
	double all_profit = 0.;//总浮动盈亏.
	pos_mutex_.Lock();
	positions_.clear();
	TradeEngine::Instance()->GetAllPosition(positions_);
	for (std::vector<PositionData>::const_iterator iter = positions_.begin(); iter != positions_.end(); ++iter) {
		float_profit_.push_back(iter->position_profit);
		all_profit += iter->position_profit;
	}
	pos_mutex_.Unlock();
	TradeEngine::Instance()->UpdateAccountProfit(all_profit);

	vector<PositionData> positions;
	TradeEngine::Instance()->GetAllPosition(positions);
	cout<<"持仓列表：\n";
	for (int i=0;i<positions.size();++i){
		cout<< ("持仓合约:") << positions[i].symbol.instrument
			<< (" 买卖:") << (positions[i].direction == LONG_DIRECTION ? "买入" : "卖空")
			<< (" 总持仓:") << positions[i].open_volume
			<< (" 昨仓:") << positions[i].yestd_volume
			<< (" 可平:") << positions[i].enable_today_volume + positions[i].enable_yestd_volume
			<< (" 持仓均价:") << positions[i].open_price<<endl;
	}
	cout<<"\n";
}
// OnOrder OnTrade
void Strategy::OnTradeEvent(TradeEventData& event){
	if (event.type == TradeEventData::ACCOUNT_EVENT) {
		UpdateAccount();
		return;
	}
	else if (event.type == TradeEventData::ORDER_EVENT)
	{
		UpdateValidOrders();
	}
	else if (event.type == TradeEventData::TRADE_EVENT)
	{
		//UpdateTrades();
		UpdatePositions();
	}
	else if (event.type == TradeEventData::POSITION_EVENT)
	{
		UpdatePositions();
	}

	//UpdateAccount();
}
// OnError
void Strategy::OnTradeError(const string &err){
	cout << err << endl;
}


