#include <boost/asio/io_context.hpp>

#include <ready_trader_go/logging.h>

#include "autotrader.h"

using namespace ReadyTraderGo;

RTG_INLINE_GLOBAL_LOGGER_WITH_CHANNEL(LG_AT, "AUTO")

constexpr signed long MIN_POSITION = -100;
constexpr signed long MAX_POSITION = 100;
constexpr unsigned long TICK_SIZE = 100;
constexpr signed long MIN_HEDGED_NET = -10;
constexpr signed long MAX_HEDGED_NET = 10;
constexpr unsigned long MIN_PRICE = (MINIMUM_BID + TICK_SIZE) / TICK_SIZE * TICK_SIZE;
constexpr unsigned long MAX_PRICE = MAXIMUM_ASK / TICK_SIZE * TICK_SIZE;

// Increase (in cents) in reservation price per increase in position; should be negative
constexpr double BIAS = -3;
// Bid-ask spread (in cents) of the inner pair; should be smaller than intended spread because of rounding
constexpr double WIDTH = 179;
// Distance between inner and outer pairs; should be a multiple of TICK_SIZE
constexpr unsigned long DIST = TICK_SIZE;
// Maximum volume of an individual order; shouldn't be too large (to avoid rejections because of volume limit and to reset hedge timer effectively)
constexpr unsigned long INNER_VOLUME = 11;
constexpr unsigned long OUTER_VOLUME = 19;
// Number of ticks between possible each hedge action
constexpr unsigned long PERIOD = 9;
// Number of lots per hedging action
constexpr unsigned long STRIDE = 7;

AutoTrader::AutoTrader(boost::asio::io_context &context) : BaseAutoTrader(context) {
    // Initialize spread
    askSpread = WIDTH / 2;
    bidSpread = -WIDTH / 2;

    // Initialize volume
    iAskVolume = INNER_VOLUME;
    iBidVolume = INNER_VOLUME;
    oAskVolume = OUTER_VOLUME;
    oBidVolume = OUTER_VOLUME;
}

void AutoTrader::DisconnectHandler() { BaseAutoTrader::DisconnectHandler(); }

void AutoTrader::ErrorMessageHandler(unsigned long id, const std::string &message) {
    if (!id) { RLOG(LG_AT, LogLevel::LL_FATAL) << "non-order error received"; return; }
    // Expect some order-related error messages, such as cross orders as we place new orders before cancelling old ones
    OrderStatusMessageHandler(id, 0, 0, 0);
}

void AutoTrader::HedgeFilledMessageHandler(unsigned long id, unsigned long price, unsigned long volume) {}

void AutoTrader::OrderBookMessageHandler(Instrument instrument, unsigned long fingerprint,
                                         const std::array<unsigned long, TOP_LEVEL_COUNT> &askPrices,
                                         const std::array<unsigned long, TOP_LEVEL_COUNT> &askVolumes,
                                         const std::array<unsigned long, TOP_LEVEL_COUNT> &bidPrices,
                                         const std::array<unsigned long, TOP_LEVEL_COUNT> &bidVolumes) {
    // Step 1: Calculate prices
    if (instrument == Instrument::ETF) return;
    if (!bidPrices[0] || !askPrices[0]) { RLOG(LG_AT, LogLevel::LL_FATAL) << "zero prices"; return; }
    double s = static_cast<double>(bidPrices[0] + askPrices[0]) / 2;
    unsigned long askPrice = (static_cast<unsigned long>(s + askSpread) + TICK_SIZE - 1) / TICK_SIZE * TICK_SIZE;
    unsigned long bidPrice = static_cast<unsigned long>(s + bidSpread) / TICK_SIZE * TICK_SIZE;

    // Step 2: Place orders
    if (iAskVolume) SendInsertOrder(nextId++, Side::SELL, askPrice, iAskVolume, Lifespan::GOOD_FOR_DAY);
    if (iBidVolume) SendInsertOrder(nextId++, Side::BUY, bidPrice, iBidVolume, Lifespan::GOOD_FOR_DAY);
    if (oAskVolume) SendInsertOrder(nextId++, Side::SELL, askPrice + DIST, oAskVolume, Lifespan::GOOD_FOR_DAY);
    if (oBidVolume) SendInsertOrder(nextId++, Side::BUY, bidPrice - DIST, oBidVolume, Lifespan::GOOD_FOR_DAY);

    // Step 3: Update records
    if (oBidVolume) { maxPosition += static_cast<signed long>(oBidVolume); orders[--nextId] = {Side::BUY, 0, oBidVolume}; }
    if (oAskVolume) { minPosition -= static_cast<signed long>(oAskVolume); orders[--nextId] = {Side::SELL, 0, oAskVolume}; }
    if (iBidVolume) { maxPosition += static_cast<signed long>(iBidVolume); orders[--nextId] = {Side::BUY, 0, iBidVolume}; }
    if (iAskVolume) { minPosition -= static_cast<signed long>(iAskVolume); orders[--nextId] = {Side::SELL, 0, iAskVolume}; }

    // Step 4: Cancel orders
    if (iAskId) { SendCancelOrder(iAskId); iAskId = 0; }
    if (iBidId) { SendCancelOrder(iBidId); iBidId = 0; }
    if (oAskId) { SendCancelOrder(oAskId); oAskId = 0; }
    if (oBidId) { SendCancelOrder(oBidId); oBidId = 0; }

    // Step 5: Update records again
    if (iAskVolume) { iAskId = nextId++; iAskVolume = 0; }
    if (iBidVolume) { iBidId = nextId++; iBidVolume = 0; }
    if (oAskVolume) { oAskId = nextId++; oAskVolume = 0; }
    if (oBidVolume) { oBidId = nextId++; oBidVolume = 0; }

    // Step 6: Hedge
    signed long net = position + hedge;
    if (MIN_HEDGED_NET <= net && net <= MAX_HEDGED_NET) { counter = 0; return; }
    if (++counter % PERIOD) return;
    if (net < 0) { SendHedgeOrder(nextId++, Side::BUY, MAX_PRICE, STRIDE); hedge += static_cast<signed long>(STRIDE); }
    else { SendHedgeOrder(nextId++, Side::SELL, MIN_PRICE, STRIDE); hedge -= static_cast<signed long>(STRIDE); }
}

void AutoTrader::OrderFilledMessageHandler(unsigned long id, unsigned long price, unsigned long volume) {}

void AutoTrader::OrderStatusMessageHandler(unsigned long id, unsigned long nFilled, unsigned long nRemaining, signed long fees) {
    // Step 1: Identify order
    auto it = orders.find(id);
    if (it == orders.cend()) { RLOG(LG_AT, LogLevel::LL_FATAL) << "order not found"; return; }
    auto &[side, oFilled, oRemaining] = it->second;

    // Step 2: Handle cancellation confirmation
    if (nFilled + nRemaining != oFilled + oRemaining) {
        if (nRemaining) { RLOG(LG_AT, LogLevel::LL_FATAL) << "inconsistent total volume"; return; }
        if (side == Side::SELL) minPosition += static_cast<signed long>(oRemaining);
        else maxPosition -= static_cast<signed long>(oRemaining);
        orders.erase(it);

        // Update volume since position bounds changed
        unsigned long maxAskVolume = static_cast<unsigned long>(minPosition - MIN_POSITION);
        unsigned long maxBidVolume = static_cast<unsigned long>(MAX_POSITION - maxPosition);
        oAskVolume = std::min(maxAskVolume, OUTER_VOLUME);
        oBidVolume = std::min(maxBidVolume, OUTER_VOLUME);
        iAskVolume = std::min(maxAskVolume - oAskVolume, INNER_VOLUME);
        iBidVolume = std::min(maxBidVolume - oBidVolume, INNER_VOLUME);
        return;
    }

    // Step 3: Handle "order placed" confirmation
    if (nFilled < oFilled) { RLOG(LG_AT, LogLevel::LL_FATAL) << "inconsistent filled volume"; return; }
    if (!nFilled && nRemaining) return;

    // Step 4: Handle filled order
    unsigned long volume = nFilled - oFilled;
    if (!volume) { RLOG(LG_AT, LogLevel::LL_FATAL) << "zero volume"; return; }
    if (side == Side::SELL) {
        position -= static_cast<signed long>(volume);
        maxPosition -= static_cast<signed long>(volume);
    } else {
        position += static_cast<signed long>(volume);
        minPosition += static_cast<signed long>(volume);
    }
    if (nRemaining == 0) {
        if (id == iAskId) iAskId = 0;
        else if (id == iBidId) iBidId = 0;
        else if (id == oAskId) oAskId = 0;
        else if (id == oBidId) oBidId = 0;
        orders.erase(it);
    } else {
        oFilled = nFilled;
        oRemaining = nRemaining;
    }

    // Update spread since position changed
    askSpread = static_cast<double>(position) * BIAS + WIDTH / 2;
    bidSpread = static_cast<double>(position) * BIAS - WIDTH / 2;

    // Update volume since position bounds changed
    unsigned long maxAskVolume = static_cast<unsigned long>(minPosition - MIN_POSITION);
    unsigned long maxBidVolume = static_cast<unsigned long>(MAX_POSITION - maxPosition);
    oAskVolume = std::min(maxAskVolume, OUTER_VOLUME);
    oBidVolume = std::min(maxBidVolume, OUTER_VOLUME);
    iAskVolume = std::min(maxAskVolume - oAskVolume, INNER_VOLUME);
    iBidVolume = std::min(maxBidVolume - oBidVolume, INNER_VOLUME);
}

void AutoTrader::TradeTicksMessageHandler(Instrument instrument, unsigned long fingerprint,
                                          const std::array<unsigned long, TOP_LEVEL_COUNT> &askPrices,
                                          const std::array<unsigned long, TOP_LEVEL_COUNT> &askVolumes,
                                          const std::array<unsigned long, TOP_LEVEL_COUNT> &bidPrices,
                                          const std::array<unsigned long, TOP_LEVEL_COUNT> &bidVolumes) {}
