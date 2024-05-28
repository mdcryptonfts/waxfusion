const { Blockchain, nameToBigInt, TimePoint, expectToThrow } = require("@eosnetwork/vert");
const { Asset, Int64, Name, UInt64, UInt128, TimePointSec } = require('@wharfkit/antelope');
const { assert } = require("chai");

function almost_equal(actual, expected, tolerance = 0.00000003) {
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

const calculate_wax_and_lswax_outputs = (wax_amount, alcor_price, real_price) => {
    const sum_of_alcor_and_real_price = alcor_price + real_price;
    const intermediate_result = wax_amount / sum_of_alcor_and_real_price;
    const lswax_output = Number(intermediate_result * real_price).toFixed(8);
    const wax_output = Number(wax_amount - lswax_output).toFixed(8);  
    return [wax_output, lswax_output];
}

const rent_cpu_memo = (receiver, wax, epoch) => {
    return `|rent_cpu|${receiver}|${wax}|${epoch}|`
}

const validate_supply_and_payouts = (snaps, swax_earning, swax_backing, lswax_supply, swax_supply, liquified_swax, log = false) => {
    let totals = { distributed: 0, swax_earning_buckets: 0, lswax_autocompounding_buckets: 0, 
        pol_buckets: 0, ecosystem_buckets: 0 }
    for(const s of snaps){
        totals.distributed += parseFloat(s.total_distributed)
        totals.swax_earning_buckets += parseFloat(s.swax_earning_bucket)
        totals.lswax_autocompounding_buckets += parseFloat(s.lswax_autocompounding_bucket)
        totals.pol_buckets += parseFloat(s.pol_bucket)
        totals.ecosystem_buckets += parseFloat(s.ecosystem_bucket)
    }

    const sum_paid_out = parseFloat(totals.swax_earning_buckets + totals.lswax_autocompounding_buckets +
        totals.pol_buckets + totals.ecosystem_buckets).toFixed(8);
    almost_equal(sum_paid_out, totals.distributed)

    //check liquified swax matches lswax supply
    assert.strictEqual(lswax_supply, liquified_swax, `lswax_supply is ${lswax_supply} but liquified swax is ${liquified_swax}`)

    //check swax earning + swax backing matches swax supply
    const expected_swax_supply = Number(parseFloat(swax_backing) + parseFloat(swax_earning)).toFixed(8)
    almost_equal(expected_swax_supply, parseFloat(swax_supply))

    if(log){
        console.log(totals)
    }
}

module.exports = {
    almost_equal,
	calculate_wax_and_lswax_outputs,
    honey,
	lswax,
	rent_cpu_memo,
	swax,
    validate_supply_and_payouts,
	wax
}