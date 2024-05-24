const { Blockchain, nameToBigInt, TimePoint, expectToThrow } = require("@eosnetwork/vert");
const { Asset, Int64, Name, UInt64, UInt128, TimePointSec } = require('@wharfkit/antelope');
const { assert } = require("chai");

//Assets
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
	lswax,
	swax,
	wax
}