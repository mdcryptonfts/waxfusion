const { Blockchain, nameToBigInt, TimePoint, expectToThrow } = require("@eosnetwork/vert");
const { Asset, Int64, Name, UInt64, UInt128, TimePointSec } = require('@wharfkit/antelope');
const { assert } = require("chai");
const blockchain = new Blockchain()
const { calculate_wax_and_lswax_outputs, lswax, rent_cpu_memo, swax, wax } = require("./helpers.ts")

const contracts = {
	alcor_contract: blockchain.createContract('swap.alcor', '../swap.alcor/build/alcor'),
 	cpu1: blockchain.createContract('cpu1.fusion', '../cpu.fusion/build/cpucontract'),
 	cpu2: blockchain.createContract('cpu2.fusion', '../cpu.fusion/build/cpucontract'),
 	cpu3: blockchain.createContract('cpu3.fusion', '../cpu.fusion/build/cpucontract'),	
 	dapp_contract: blockchain.createContract('dapp.fusion', 'build/fusion'),
    honey_contract: blockchain.createContract('nfthivehoney', '../eosio.token/build/token'),
 	pol_contract: blockchain.createContract('pol.fusion', '../pol.fusion/build/polcontract'),
 	stake_contract: blockchain.createContract('eosio.stake', '../mock.stake/build/stake'),
 	system_contract: blockchain.createContract('eosio', '../mock.system/build/system'),
 	token_contract: blockchain.createContract('token.fusion', '../token.fusion/build/token'),
 	voter_contract: blockchain.createContract('eosio.voters', '../mock.voters/build/voters'),
 	wax_contract: blockchain.createContract('eosio.token', '../eosio.token/build/token')
}



const initial_state = {
	alcor_lswax_pool: lswax(95300),
    alcor_wax_pool: wax(100000),
    chain_time: 1710460800,
    circulating_wax: `10000000.00000000 WAX`,
    dapp_rental_pool: Number(calculate_wax_and_lswax_outputs(100000, 100000 / 95300, 1)[1]), //48796.72299027
    honey_supply: `461168601842738.7903 HONEY`,
    swax_backing_lswax: Number(calculate_wax_and_lswax_outputs(100000, 100000 / 95300, 1)[1]), //48796.72299027  
    swax_supply: `46116860184.27387903 SWAX`,
    lswax_supply: `46116860184.27387903 LSWAX`,
    wax_supply: `46116860184.27387903 WAX`
}

const setTime = async (seconds) => {
    const time = TimePointSec.fromInteger(seconds);
    return blockchain.setTime(time);    
}

function incrementTime(seconds = TEN_MINUTES) {
    const time = TimePointSec.fromInteger(seconds);
    return blockchain.addTime(time);
}

const init = async () => {
	await setTime(initial_state.chain_time);
    await contracts.system_contract.actions.initproducer().send();
    await contracts.dapp_contract.actions.initconfig3().send();
    await contracts.dapp_contract.actions.initconfig().send();
    await contracts.dapp_contract.actions.initstate2().send();
    await contracts.dapp_contract.actions.initstate3().send();
    await contracts.dapp_contract.actions.inittop21().send();
    await contracts.pol_contract.actions.initconfig().send();
    await contracts.pol_contract.actions.initstate3().send();   
    await contracts.cpu1.actions.initstate().send();
    await contracts.cpu2.actions.initstate().send();
    await contracts.cpu3.actions.initstate().send();  
    await contracts.alcor_contract.actions.initunittest([initial_state.alcor_wax_pool, initial_state.alcor_lswax_pool]).send();       
    await contracts.alcor_contract.actions.createpool(['eosio', {quantity: '0.0000 HONEY', contract: 'nfthivehoney'}, {quantity: lswax(0), contract: 'token.fusion'}]).send('eosio@active');       
    await contracts.wax_contract.actions.create(['eosio', initial_state.wax_supply]).send();
    await contracts.honey_contract.actions.create(['mike', initial_state.honey_supply]).send();
    await contracts.honey_contract.actions.issue(['mike', initial_state.honey_supply, 'issuing honey']).send('mike@active');
    await contracts.wax_contract.actions.issue(['eosio', initial_state.wax_supply, 'issuing wax']).send('eosio@active');
    await contracts.token_contract.actions.create(['dapp.fusion', initial_state.swax_supply]).send();
    await contracts.token_contract.actions.create(['dapp.fusion', initial_state.lswax_supply]).send();
    await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(1000000), '1M wax for mike']).send('eosio@active');
    await contracts.wax_contract.actions.transfer(['eosio', 'bob', wax(1000000), '1M wax for bob']).send('eosio@active');
    await contracts.wax_contract.actions.transfer(['eosio', 'ricky', wax(1000000), '1M wax for ricky']).send('eosio@active');

    //deposit initial 100k liquidity to POL, and 100k initial staking pool funds
    await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(100000), 'for liquidity only']).send('eosio@active');
    await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(100000), 'for staking pool only']).send('eosio@active');

    //add incentive to incentives table
    await contracts.dapp_contract.actions.setincentive([2, '8,WAX', 'eosio.token', 25000000]).send('dapp.fusion@active')
}

const stake = async (user, amount, liquify = false, liquify_amount = amount) => {
    const action = await contracts.dapp_contract.actions.stake([user]).send(`${user}@active`);
    await contracts.wax_contract.actions.transfer([user, 'dapp.fusion', wax(amount), 'stake']).send(`${user}@active`);
    if(liquify){
        await contracts.dapp_contract.actions.liquify([user, swax(liquify_amount)]).send(`${user}@active`);
    }
}

const unliquify = async (user, amount) => {
    await contracts.token_contract.actions.transfer([user, 'dapp.fusion', lswax(amount), 'unliquify']).send(`${user}@active`);
}

const simulate_days = async (days = 1, stake_users = false, claim_rewards = true, rent_cpu = true, current_time = initial_state.chain_time) => {

    let count = 0;
    while(count < days){
        //mike stakes 10000 swax, bob stake 10k and liquifies 5k, ricky stakes 10k and liquifies it all    
        if(stake_users){
            await stake('mike', 10000)
            await stake('bob', 10000, true, 5000)
            await stake('ricky', 10000, true)
        }

        if(rent_cpu){
            if(count < 10){
                const memo = rent_cpu_memo('ricky', 100, initial_state.chain_time)
                await contracts.wax_contract.actions.transfer(['ricky', 'dapp.fusion', wax(10), memo]).send('ricky@active')
            } else {
                const memo = rent_cpu_memo('ricky', 100, initial_state.chain_time + (86400 * 7) )
                await contracts.wax_contract.actions.transfer(['ricky', 'dapp.fusion', wax(10), memo]).send('ricky@active')
            }
        }

        //fast forward a day
        current_time += 86400
        await setTime(current_time)

        //claim rewards
        if(claim_rewards){
            if(count < 10){
                await contracts.dapp_contract.actions.claimgbmvote(['cpu1.fusion']).send('mike@active')
            } else {
                await contracts.dapp_contract.actions.claimgbmvote(['cpu2.fusion']).send('mike@active')
            }
        }
        
        //distribute
        await contracts.dapp_contract.actions.distribute([]).send('mike@active')
  
        count ++

    }
}

module.exports = {
	blockchain,
	contracts,
    incrementTime,
	init,
    initial_state,
    setTime,
    stake,
    simulate_days,
    unliquify
}