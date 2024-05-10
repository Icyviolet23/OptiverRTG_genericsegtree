#ifndef CPPREADY_TRADER_GO_AUTOTRADER_H
#define CPPREADY_TRADER_GO_AUTOTRADER_H

#include <map>
#include <unordered_map>

#include <boost/asio/io_context.hpp>

#include <ready_trader_go/baseautotrader.h>
#include <ready_trader_go/types.h>

class AutoTrader : public ReadyTraderGo::BaseAutoTrader {
public:
    explicit AutoTrader(boost::asio::io_context &context);

    void DisconnectHandler() override;

    void ErrorMessageHandler(unsigned long id, const std::string &message) override;

    void HedgeFilledMessageHandler(unsigned long id, unsigned long price, unsigned long volume) override;

    void OrderBookMessageHandler(ReadyTraderGo::Instrument instrument,
                                 unsigned long fingerprint,
                                 const std::array<unsigned long, ReadyTraderGo::TOP_LEVEL_COUNT> &askPrices,
                                 const std::array<unsigned long, ReadyTraderGo::TOP_LEVEL_COUNT> &askVolumes,
                                 const std::array<unsigned long, ReadyTraderGo::TOP_LEVEL_COUNT> &bidPrices,
                                 const std::array<unsigned long, ReadyTraderGo::TOP_LEVEL_COUNT> &bidVolumes) override;

    void OrderFilledMessageHandler(unsigned long id, unsigned long price, unsigned long volume) override;

    void OrderStatusMessageHandler(unsigned long id, unsigned long nFilled, unsigned long nRemaining, signed long fees) override;

    void TradeTicksMessageHandler(ReadyTraderGo::Instrument instrument,
                                  unsigned long fingerprint,
                                  const std::array<unsigned long, ReadyTraderGo::TOP_LEVEL_COUNT> &askPrices,
                                  const std::array<unsigned long, ReadyTraderGo::TOP_LEVEL_COUNT> &askVolumes,
                                  const std::array<unsigned long, ReadyTraderGo::TOP_LEVEL_COUNT> &bidPrices,
                                  const std::array<unsigned long, ReadyTraderGo::TOP_LEVEL_COUNT> &bidVolumes) override;

private:
    unsigned long nextId = 1;
    unsigned long iBidId = 0;
    unsigned long iAskId = 0;
    unsigned long oBidId = 0;
    unsigned long oAskId = 0;

    signed long position = 0;
    signed long minPosition = 0;
    signed long maxPosition = 0;

    double bidSpread = 0;
    double askSpread = 0;

    unsigned long iBidVolume = 0;
    unsigned long iAskVolume = 0;
    unsigned long oBidVolume = 0;
    unsigned long oAskVolume = 0;

    signed long hedge = 0;
    unsigned long counter = 0;

    // all orders (id -> type * filled * remaining)
    std::unordered_map<unsigned long, std::tuple<ReadyTraderGo::Side, unsigned long, unsigned long>> orders;
};

#endif //CPPREADY_TRADER_GO_AUTOTRADER_H
#ifndef CPPREADY_TRADER_GO_AUTOTRADER_H
#define CPPREADY_TRADER_GO_AUTOTRADER_H

#include <map>
#include <unordered_map>

#include <boost/asio/io_context.hpp>

#include <ready_trader_go/baseautotrader.h>
#include <ready_trader_go/types.h>

class AutoTrader : public ReadyTraderGo::BaseAutoTrader {
public:
    explicit AutoTrader(boost::asio::io_context &context);

    void DisconnectHandler() override;

    void ErrorMessageHandler(unsigned long id, const std::string &message) override;

    void HedgeFilledMessageHandler(unsigned long id, unsigned long price, unsigned long volume) override;

    void OrderBookMessageHandler(ReadyTraderGo::Instrument instrument,
                                 unsigned long fingerprint,
                                 const std::array<unsigned long, ReadyTraderGo::TOP_LEVEL_COUNT> &askPrices,
                                 const std::array<unsigned long, ReadyTraderGo::TOP_LEVEL_COUNT> &askVolumes,
                                 const std::array<unsigned long, ReadyTraderGo::TOP_LEVEL_COUNT> &bidPrices,
                                 const std::array<unsigned long, ReadyTraderGo::TOP_LEVEL_COUNT> &bidVolumes) override;

    void OrderFilledMessageHandler(unsigned long id, unsigned long price, unsigned long volume) override;

    void OrderStatusMessageHandler(unsigned long id, unsigned long nFilled, unsigned long nRemaining, signed long fees) override;

    void TradeTicksMessageHandler(ReadyTraderGo::Instrument instrument,
                                  unsigned long fingerprint,
                                  const std::array<unsigned long, ReadyTraderGo::TOP_LEVEL_COUNT> &askPrices,
                                  const std::array<unsigned long, ReadyTraderGo::TOP_LEVEL_COUNT> &askVolumes,
                                  const std::array<unsigned long, ReadyTraderGo::TOP_LEVEL_COUNT> &bidPrices,
                                  const std::array<unsigned long, ReadyTraderGo::TOP_LEVEL_COUNT> &bidVolumes) override;

private:
    unsigned long nextId = 1;
    unsigned long iBidId = 0;
    unsigned long iAskId = 0;
    unsigned long oBidId = 0;
    unsigned long oAskId = 0;

    signed long position = 0;
    signed long minPosition = 0;
    signed long maxPosition = 0;

    double bidSpread = 0;
    double askSpread = 0;

    unsigned long iBidVolume = 0;
    unsigned long iAskVolume = 0;
    unsigned long oBidVolume = 0;
    unsigned long oAskVolume = 0;

    signed long hedge = 0;
    unsigned long counter = 0;

    // all orders (id -> type * filled * remaining)
    std::unordered_map<unsigned long, std::tuple<ReadyTraderGo::Side, unsigned long, unsigned long>> orders;
};

#endif //CPPREADY_TRADER_GO_AUTOTRADER_H
