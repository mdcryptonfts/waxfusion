const { blockchain, contracts, incrementTime, init, initial_state, setTime, stake, simulate_days, unliquify } = require("./setup.spec.ts")
const { calculate_wax_and_lswax_outputs, honey, lswax, rent_cpu_memo, swax, validate_supply_and_payouts, wax } = require("./helpers.ts")
const { nameToBigInt, TimePoint, expectToThrow } = require("@eosnetwork/vert");
const { Asset, Int64, Name, UInt64, UInt128, TimePointSec } = require('@wharfkit/antelope');
const { assert } = require("chai");

const [mike, bob, ricky] = blockchain.createAccounts('mike', 'bob', 'ricky')


/* Runs before each test */
beforeEach(async () => {
    blockchain.resetTables()
    await init()
})

const scopes = {
    alcor: contracts.alcor_contract.value,
    dapp: contracts.dapp_contract.value,
    cpu1: contracts.cpu1.value,
    cpu2: contracts.cpu2.value,
    cpu3: contracts.cpu3.value,
    pol: contracts.pol_contract.value,
    system: contracts.system_contract.value
}

const getPayouts = async () => {
    const payouts = await contracts.dapp_contract.tables
        .payouts(scopes.dapp)
        .getTableRows()
        console.log("payouts:")
        console.log(payouts) 

}

const getAlcorPool = async (log = false) => {
    const alcor_pool = await contracts.alcor_contract.tables
        .pools(scopes.alcor)
        .getTableRows(BigInt( initial_state.alcor_pool_id ))[0]
    if(log){
        console.log("alcor pool:")
        console.log(alcor_pool) 
    }  
    return alcor_pool 
}

const getBalances = async (user, contract, log = false) => {
    const scope = Name.from(user).value.value
    const rows = await contract.tables
        .accounts(scope)
        .getTableRows()
    if(log){
        console.log(`${user}'s balances:`)
        console.log(rows)
    }
    return rows
}

const getDappConfig = async (log = false) => {
    const dapp_config = await contracts.dapp_contract.tables
        .config3(scopes.dapp)
        .getTableRows()[0]
    if(log){
        console.log('dapp config:')
        console.log(dapp_config)    
    }
    return dapp_config
}

const getDappState = async (log = false) => {
    const state = await contracts.dapp_contract.tables
        .state(scopes.dapp)
        .getTableRows()[0]
    if(log){
        console.log('dapp state:')
        console.log(state)
    }
    return state 
}

const getDappState2 = async (log = false) => {
    const state2 = await contracts.dapp_contract.tables
        .state2(scopes.dapp)
        .getTableRows()[0]
    if(log){
        console.log('dapp state2:')
        console.log(state2)
    }
    return state2 
}

const getDappTop21 = async (log = false) => {
    const top21 = await contracts.dapp_contract.tables
        .top21(scopes.dapp)
        .getTableRows()[0]
    if(log){
        console.log('top21:')
        console.log(top21)  
    }
    return top21    
}

const getPolConfig = async (log = false) => {
    const pol_config = await contracts.pol_contract.tables
        .config2(scopes.pol)
        .getTableRows()[0]
    if(log){
        console.log('pol config:')
        console.log(pol_config)    
    }
    return pol_config
}

const getPolState = async (log = false) => {
    const pol_state = await contracts.pol_contract.tables
        .state3(scopes.pol)
        .getTableRows()[0]
    if(log){
        console.log('pol state')
        console.log(pol_state)  
    }
    return pol_state;  
}

const getRenters = async (log = false) => {
    const renters = await contracts.pol_contract.tables
        .renters(scopes.pol)
        .getTableRows()
    if(log){
        console.log("renters:")
        console.log(renters)
    }
    return renters; 
}

const getSnapshots = async (log = false) => {
    const snaps = await contracts.dapp_contract.tables
        .snapshots(scopes.dapp)
        .getTableRows()
    if(log){
        console.log('snapshots')
        console.log(snaps)
    }
    return snaps 
}

const getSupply = async (account, token, log = false) => {
    const scope = Asset.SymbolCode.from(token).value.value
    const row = await account.tables
        .stat(scope)
        .getTableRows()[0]
    if(log){
        console.log(`${token} supply:`)
        console.log(row)
    }
    return row;
}

const getSWaxStaker = async (user, log = false) => {
    const staker = await contracts.dapp_contract.tables
        .stakers(scopes.dapp)
        .getTableRows(Name.from(user).value.value)[0]
    if(log){
        console.log(`${user}'s SWAX:`)
        console.log(staker)
    }
    return staker; 
}

/* Tests */
describe('\n\nverify initial state and config', () => {
    //pass `true` to log the results in the console
    it('', async () => {
        await getDappConfig(true)
        await getDappState(true)
        await getDappTop21(true) 
    });  
});

describe('\n\nstake memo', () => {

    it('error: need to use the stake action first', async () => {
    	const action = contracts.wax_contract.actions.transfer(['mike', 'dapp.fusion', wax(10), 'stake']).send('mike@active');
    	await expectToThrow(action, "eosio_assert: you need to use the stake action first")
    }); 

    it('error: minimum amount not met', async () => {
        const action = contracts.wax_contract.actions.transfer(['mike', 'dapp.fusion', wax(0.1), 'stake']).send('mike@active');
        await expectToThrow(action, "eosio_assert: minimum stake amount not met")
    });     

    it('error: wrong token', async () => {
        await stake('mike', 10, true)
        const action = contracts.token_contract.actions.transfer(['mike', 'dapp.fusion', lswax(0.1), 'stake']).send('mike@active');
        await expectToThrow(action, "eosio_assert: only WAX is used for staking")
    });       

    it('success: staked 10 wax', async () => {
        await stake('mike', 10)
    	const dapp_state = await getDappState();
    	assert(dapp_state.swax_currently_earning == swax(10), "swax_currently_earning should be 10");
        assert(dapp_state.wax_available_for_rentals == wax(initial_state.dapp_rental_pool + 10), `wax_available_for_rentals should be ${initial_state.dapp_rental_pool + 10}`);
    	const swax_supply = await getSupply(contracts.token_contract, "SWAX");
    	assert(swax_supply.supply == swax(initial_state.dapp_rental_pool + 10), `swax supply should be ${initial_state.dapp_rental_pool + 10}`);
    });     
});


describe('\n\nunliquify memo', () => {

    it('error: need to use the stake action first', async () => {
        await stake('mike', 100, true)
        await contracts.token_contract.actions.transfer(['mike', 'ricky', lswax(10), '']).send('mike@active');
        const action = contracts.token_contract.actions.transfer(['ricky', 'dapp.fusion', lswax(10), 'unliquify']).send('ricky@active');
        await expectToThrow(action, "eosio_assert: you need to use the stake action first")
    });

    it('error: wrong token sent', async () => {
        await stake('mike', 10, true, 0.9)
        const action = contracts.wax_contract.actions.transfer(['mike', 'dapp.fusion', wax(1), 'unliquify']).send('mike@active');
        await expectToThrow(action, "eosio_assert: only LSWAX can be unliquified")
    });      

    it('error: minimum not met', async () => {
        await stake('mike', 10, true, 0.9)
        const action = contracts.token_contract.actions.transfer(['mike', 'dapp.fusion', lswax(0.9), 'unliquify']).send('mike@active');
        await expectToThrow(action, "eosio_assert: minimum unliquify amount not met")
    });  

    it('success, unliquified 10 lswax', async () => {
        await stake('mike', 10, true)
        await contracts.token_contract.actions.transfer(['mike', 'dapp.fusion', lswax(10), 'unliquify']).send('mike@active');
        const bal = await getSWaxStaker('mike')
        assert(bal.swax_balance == swax(10), "mike should have 10 SWAX")
    });  

});

describe('\n\nwaxfusion_revenue memo', () => {

    it('error: only wax accepted', async () => {
        await stake('mike', 10, true)
        const action = contracts.token_contract.actions.transfer(['mike', 'dapp.fusion', lswax(10), 'waxfusion_revenue']).send('mike@active');
        await expectToThrow(action, "eosio_assert: only WAX is accepted with waxfusion_revenue memo")
    });   

    it('success, 100k wax deposited', async () => {
        await contracts.wax_contract.actions.transfer(['mike', 'dapp.fusion', wax(100000), 'waxfusion_revenue']).send('mike@active');
        const dapp_state = await getDappState()
        assert(dapp_state.revenue_awaiting_distribution == wax(100000), "dapp revenue should be 100k wax")
    }); 
});

describe('\n\nlp_incentives memo', () => {

    it('success, 100k wax deposited', async () => {
        await contracts.wax_contract.actions.transfer(['mike', 'dapp.fusion', wax(100000), 'lp_incentives']).send('mike@active');
        const dapp_state2 = await getDappState2()
        assert(dapp_state2.incentives_bucket == lswax(100000), "incentives bucket should be 100k lswax")
    }); 

    it('success, 1k lsWAX deposited', async () => {
        await stake('mike', 1000, true)
        await contracts.token_contract.actions.transfer(['mike', 'dapp.fusion', lswax(1000), 'lp_incentives']).send('mike@active');
        const dapp_state2 = await getDappState2()
        assert(dapp_state2.incentives_bucket == lswax(1000), "incentives bucket should be 1000 lswax")
    });     
});


describe('\n\ncpu rental return memo', () => {

    it('error: invalid sender', async () => {
        const action = contracts.wax_contract.actions.transfer(['mike', 'dapp.fusion', wax(100), 'cpu rental return']).send('mike@active');
        await expectToThrow(action, "eosio_assert: sender is not a valid cpu rental contract")
    });  
 
});

describe('\n\nrent_cpu memo', () => {

    it('error: only wax accepted', async () => {
        await stake('ricky', 1000, true)
        const memo = rent_cpu_memo('ricky', 100, initial_state.chain_time)
        const action = contracts.token_contract.actions.transfer(['ricky', 'dapp.fusion', lswax(10), memo]).send('ricky@active') 
        await expectToThrow(action, "eosio_assert: only WAX can be sent with this memo")
    });  
 
});


describe('\n\nunliquify_exact memo', () => {

    it('error: only lswax accepted', async () => {

    });  
 
});

/*
describe('\n\nsimulate_days', () => {

    it('success', async () => {
        //await stake('bob', 10000, true, 5000)
        //await stake('mike', 10000)
        await simulate_days(14, true)

        //check the dapp state
        //await getDappState(true)

        //check the pol state
        //await getPolState(true)

        //swax backing lswax should now be initial + (5*simulation)

        //check the staked balance of each user

        //check the lswax balance of each user

        //check the lswax supply
        await getSupply(contracts.token_contract, "LSWAX")

        //check the swax supply
        await getSupply(contracts.token_contract, "SWAX")

        //await unliquify('bob', 10000);
        //const bobs_bal = await getBalances('bob', contracts.token_contract, true)

        //try using pol contract to mess things up (instant redeem, rebalance)
        //await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(10000), '']).send('mike@active')
        //await contracts.token_contract.actions.transfer(['bob', 'pol.fusion', lswax(10000), '']).send('bob@active')
        //await contracts.pol_contract.actions.rebalance([]).send('mike@active')

        //check the dapp state
        const dapp_state = await getDappState()

        //check the pol state
        //await getPolState(true)

        //check the lswax supply
        const lswax_supply = await getSupply(contracts.token_contract, "LSWAX")

        //check the swax supply
        const swax_supply = await getSupply(contracts.token_contract, "SWAX")

        const snaps = await getSnapshots()
        validate_supply_and_payouts(snaps, dapp_state.swax_currently_earning, dapp_state.swax_currently_backing_lswax,
                lswax_supply.supply, swax_supply.supply, dapp_state.liquified_swax )

        await incrementTime(1)
        const bobs_swax = await getSWaxStaker('bob')
        const mikes_swax = await getSWaxStaker('mike')
        const rickys_swax = await getSWaxStaker('ricky')
        await contracts.dapp_contract.actions.claimrewards(['bob']).send('bob@active')
        await contracts.dapp_contract.actions.claimrewards(['mike']).send('mike@active')
        const bobs_swax_after = await getSWaxStaker('bob')
        const bobs_wax_after = await getBalances('bob', contracts.wax_contract)
        const mikes_swax_after = await getSWaxStaker('mike')
        const mikes_wax_after = await getBalances('mike', contracts.wax_contract)        
        validate_supply_and_payouts(snaps, dapp_state.swax_currently_earning, dapp_state.swax_currently_backing_lswax,
                lswax_supply.supply, swax_supply.supply, dapp_state.liquified_swax )    
        //await getPayouts()    
    });  
 
});
*/

describe('\n\nno memo / unexpected memo', () => {
    const err = `eosio_assert: must include a memo for transfers to dapp.fusion, see docs.waxfusion.io for a list of memos`
    it('honey was received', async () => {
        await contracts.honey_contract.actions.transfer(['mike', 'dapp.fusion', honey(10), '']).send('mike@active');
    });  

    it('wax was received', async () => {
        const action = contracts.wax_contract.actions.transfer(['mike', 'dapp.fusion', wax(10), 's']).send('mike@active');
        await expectToThrow(action, err)
    });     

    it('lswax was received', async () => {
        await stake('mike', 100, true)
        const action = contracts.token_contract.actions.transfer(['mike', 'dapp.fusion', lswax(10), '']).send('mike@active');
        await expectToThrow(action, err)
    });    
});

module.exports = {
    getPayouts
}