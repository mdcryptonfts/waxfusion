const { Blockchain, nameToBigInt, TimePoint, expectToThrow } = require("@eosnetwork/vert");
const { Asset, Int64, Name, UInt64, UInt128, TimePointSec } = require('@wharfkit/antelope');
const { assert } = require("chai");
const { lswax, swax, wax } = require('./helpers.ts')
const blockchain = new Blockchain()

const contracts = {
	alcor_contract: blockchain.createContract('swap.alcor', '../swap.alcor/build/alcor'),
 	cpu1: blockchain.createContract('cpu1.fusion', '../cpu.fusion/build/cpucontract'),
 	cpu2: blockchain.createContract('cpu2.fusion', '../cpu.fusion/build/cpucontract'),
 	cpu3: blockchain.createContract('cpu3.fusion', '../cpu.fusion/build/cpucontract'),	
 	dapp_contract: blockchain.createContract('dapp.fusion', '../dapp.fusion/build/fusion'),
 	pol_contract: blockchain.createContract('pol.fusion', 'build/polcontract'),
 	stake_contract: blockchain.createContract('eosio.stake', '../mock.stake/build/stake'),
 	system_contract: blockchain.createContract('eosio', '../mock.system/build/system'),
 	token_contract: blockchain.createContract('token.fusion', '../token.fusion/build/token'),
 	voter_contract: blockchain.createContract('eosio.voters', '../mock.voters/build/voters'),
 	wax_contract: blockchain.createContract('eosio.token', '../eosio.token/build/token')
}

const initial_state = {
	alcor_lswax_pool: `95300.00000000 LSWAX`,
    alcor_pool_id: 2,
    alcor_wax_pool: `100000.00000000 WAX`,
    chain_time: 1710460800,
    circulating_wax: `10000000.00000000 WAX`,
    reward_pool: `1000.00000000 WAX`,
    swax_supply: `46116860184.27387903 SWAX`,
    lswax_supply: `46116860184.27387903 LSWAX`,
    wax_supply: `46116860184.27387903 WAX`
}

const initial_alcor_price = () => {
    return parseFloat(100.0 / 95.3).toFixed(8);
}

const setTime = async (seconds) => {
    const time = TimePointSec.fromInteger(seconds);
    return blockchain.setTime(time);    
}

function incrementTime(seconds = 86400) {
    const time = TimePointSec.fromInteger(seconds);
    return blockchain.addTime(time);
}

const init = async () => {
	await setTime(initial_state.chain_time);

    await contracts.wax_contract.actions.create(['eosio', initial_state.wax_supply]).send();
    await contracts.wax_contract.actions.issue(['eosio', initial_state.wax_supply, 'issuing wax']).send('eosio@active');
    await contracts.wax_contract.actions.transfer(['eosio', 'dapp.fusion', initial_state.reward_pool, '']).send('eosio@active');
    await contracts.dapp_contract.actions.init([initial_state.reward_pool]).send();
    await contracts.alcor_contract.actions.initunittest([initial_state.alcor_wax_pool, initial_state.alcor_lswax_pool]).send();
    await contracts.system_contract.actions.initproducer().send();
    await contracts.dapp_contract.actions.inittop21().send();
    await contracts.pol_contract.actions.initconfig([initial_state.alcor_pool_id]).send();
    await contracts.pol_contract.actions.initstate3().send();   
    await contracts.cpu1.actions.initstate().send();
    await contracts.cpu2.actions.initstate().send();
    await contracts.cpu3.actions.initstate().send();  
    await contracts.token_contract.actions.create(['dapp.fusion', initial_state.swax_supply]).send();
    await contracts.token_contract.actions.create(['dapp.fusion', initial_state.lswax_supply]).send();
}

const stake = async (user, amount, liquify = false, liquify_amount = amount) => {
    await contracts.dapp_contract.actions.stake([user]).send(`${user}@active`);
    await contracts.wax_contract.actions.transfer([user, 'dapp.fusion', wax(amount), 'stake']).send(`${user}@active`);
    if(liquify){
        await contracts.dapp_contract.actions.liquify([user, swax(liquify_amount)]).send(`${user}@active`);
    }
}

module.exports = {
	blockchain,
	contracts,
	init,
    initial_alcor_price,
    initial_state,
    setTime,
    incrementTime,
    stake
}