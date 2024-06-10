const { Blockchain, nameToBigInt, TimePoint, expectToThrow } = require("@eosnetwork/vert");
const { Asset, Int64, Name, UInt64, UInt128, TimePointSec } = require('@wharfkit/antelope');
const { assert } = require("chai");

function almost_equal(actual, expected, tolerance = 0.000003) {
    const difference = Math.abs(actual - expected);
    const relativeError = difference / Math.abs(expected);
    assert.isTrue(relativeError <= tolerance, `Expected ${actual} to be within ${tolerance * 100}% of ${expected}`);
}

//Assets
const honey = (amount) => {
    return `${parseFloat(amount).toFixed(4)} HONEY`
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

//convert the sqrtPriceX64 from alcor into actual asset prices for tokenA and tokenB
const sqrt64_to_price = (sqrtPriceX64) => {

    let sqrtPrice = (sqrtPriceX64 * 10000) / (2 ** 64)
    let price = sqrtPrice * sqrtPrice;
    let P_tokenA_128 = price;
    let P_tokenB_128 = (10 ** 16) / P_tokenA_128;

}

const calculate_wax_and_lswax_outputs = (wax_amount, alcor_price, real_price) => {
    let sqrtPrice = 18008028479818450617;
    sqrt64_to_price(sqrtPrice)
    const sum_of_alcor_and_real_price = alcor_price + real_price;
    const intermediate_result = wax_amount / sum_of_alcor_and_real_price;

    const lswax_output = Number(intermediate_result * real_price).toFixed(8);
    const wax_output = Number(wax_amount - lswax_output).toFixed(8);  

    return [wax_output, lswax_output];
}

const rent_cpu_memo = (receiver, wax, epoch) => {
    return `|rent_cpu|${receiver}|${wax}|${epoch}|`
}


module.exports = {
    almost_equal,
	calculate_wax_and_lswax_outputs,
    honey,
	lswax,
	rent_cpu_memo,
	swax,
	wax
}