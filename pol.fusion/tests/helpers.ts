const { Blockchain, nameToBigInt, TimePoint, expectToThrow } = require("@eosnetwork/vert");
const { Asset, Int64, Name, UInt64, UInt128, TimePointSec } = require('@wharfkit/antelope');
const { assert } = require("chai");

//since we are using javascript
//allow anything within 0.000003% of the expected result to be tolerated after doing calculations
function almost_equal(actual, expected, tolerance = 0.00000003) {
    const difference = Math.abs(actual - expected);
    const relativeError = difference / Math.abs(expected);
    assert.isTrue(relativeError <= tolerance, `Expected ${actual} to be within ${tolerance * 100}% of ${expected}`);
}

const calculate_lswax_to_match_wax = (wax_amount, alcor_price, real_price) => {
    const wax_cost_on_alcor = wax_amount / alcor_price;
    const lswax_received = wax_cost_on_alcor / real_price;
    return parseFloat(lswax_received).toFixed(8)
}

const calculate_wax_and_lswax_outputs = (wax_amount, alcor_price, real_price) => {
    const sum_of_alcor_and_real_price = alcor_price + real_price;
    const intermediate_result = wax_amount / sum_of_alcor_and_real_price;
    const lswax_output = intermediate_result * real_price;
    const wax_output = wax_amount - lswax_output;  
    return [wax_output, lswax_output];
}

const calculate_wax_to_match_lswax = (lswax_amount, alcor_price, real_price) => {
    //multiply the lswax amount by alcors price
    const lswax_cost_on_alcor = lswax_amount * alcor_price;

    //multiply that by the real price
    const cost_of_quantity_on_dapp = lswax_cost_on_alcor * real_price;

    return parseFloat(cost_of_quantity_on_dapp).toFixed(8)
}

const extend_rental_memo = (receiver, days) => {
    return `|extend_rental|${receiver}|${days}|`
}

const increase_rental_memo = (receiver, days, wax) => {
    return `|increase_rental|${receiver}|${days}|${wax}|`
}

const rent_cpu_memo = (receiver, days, wax) => {
    return `|rent_cpu|${receiver}|${days}|${wax}|`
}

const lswax = (amount) => {
    return `${parseFloat(amount).toFixed(8)} LSWAX`
}

const swax = (amount) => {
    return `${parseFloat(amount).toFixed(8)} SWAX`
}

const wax = (amount) => {
    return `${parseFloat(amount).toFixed(8)} WAX`
}


module.exports = {
    almost_equal,
    calculate_lswax_to_match_wax,
    calculate_wax_and_lswax_outputs,
    calculate_wax_to_match_lswax,
    extend_rental_memo,
    increase_rental_memo,
    rent_cpu_memo,
    lswax,
    swax,
    wax
}