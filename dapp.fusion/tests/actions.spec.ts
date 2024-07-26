const { blockchain, contracts, incrementTime, init, initial_state, setTime, stake, simulate_days, unliquify } = require("./setup.spec.ts")
const { getPayouts } = require('./notifications.spec.ts')
const { almost_equal, calculate_wax_and_lswax_outputs, lswax, rent_cpu_memo, swax, wax } = require("./helpers.ts")
const { nameToBigInt, TimePoint, expectToThrow } = require("@eosnetwork/vert");
const { Asset, Int64, Name, UInt64, UInt128, TimePointSec } = require('@wharfkit/antelope');
const { assert } = require("chai");

const [mike, bob, ricky, oig] = blockchain.createAccounts('mike', 'bob', 'ricky', 'oig')


/* Runs before each test */
beforeEach(async () => {
    blockchain.resetTables()
    await init()
})

/* Runs after each test */
afterEach(async () => {
    await verifyState()
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

const getAlcorPool = async (log = false, id = 2) => {
    const alcor_pool = await contracts.alcor_contract.tables
        .pools(scopes.alcor)
        .getTableRows(BigInt( id ))[0]
    if(log){
        console.log("alcor pool:")
        console.log(alcor_pool) 
    }  
    return alcor_pool 
}

const getAlcorIncentives = async (log = false) => {
    const incentives = await contracts.alcor_contract.tables
        .incentives(scopes.alcor)
        .getTableRows()
    if(log){
        console.log("alcor incentives:")
        console.log(incentives) 
    }  
    return incentives 
}

const getDappIncentives = async (log = false) => {
    const incentives = await contracts.dapp_contract.tables
        .lpfarms(scopes.dapp)
        .getTableRows()
    if(log){
        console.log("dapp incentives:")
        console.log(incentives) 
    }  
    return incentives 
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

const getDappGlobal = async (log = false) => {
    const g = await contracts.dapp_contract.tables
        .global(scopes.dapp)
        .getTableRows()[0]
    if(log){
        console.log('global:')
        console.log(g)  
    }
    return g    
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


const getEpochs = async (log = false) => {
    const epochs = await contracts.dapp_contract.tables
        .epochs(scopes.dapp)
        .getTableRows()
    if(log){
        console.log('epochs:')
        console.log(epochs)  
    }
    return epochs    
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

const getRedemptionRequests = async (user, log = false) => {
    const requests = await contracts.dapp_contract.tables
        .rdmrequests(Name.from(user).value.value)
        .getTableRows()
    if(log){
        console.log(`${user}'s requests:`)
        console.log(requests)
    }
    return requests; 
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

const getRewardFarm = async (log = false) => {
    const r = await contracts.dapp_contract.tables
        .rewards(scopes.dapp)
        .getTableRows()[0]
    if(log){
        console.log('rewards:')
        console.log(r)  
    }
    return r  
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

const getAllStakers = async (log = false) => {
    const staker = await contracts.dapp_contract.tables
        .stakers(scopes.dapp)
        .getTableRows()
    if(log){
        console.log(`all swax holders:`)
        console.log(staker)
    }
    return staker; 
}

const getDebug = async (log = false) => {
    const debug = await contracts.dapp_contract.tables
        .debug(scopes.dapp)
        .getTableRows()
    if(log){
        console.log(debug)
    }
    return debug; 
}

const getVoters = async (log = false) => {
    const voters = await contracts.system_contract.tables
        .voters(scopes.system)
        .getTableRows()
    if(log){
        console.log(`voters:`)
        console.log(voters)
    }
    return voters; 
}

const getDelBw = async (account, log = false) => {
    const delbw = await contracts.system_contract.tables
        .delband(Name.from(account).value.value)
        .getTableRows()
    if(log){
        console.log('delbw:')
        console.log(delbw)
    }
    return delbw 
}

const verifyState = async (log = false) => {
    const lswax_supply = await getSupply( contracts.token_contract, 'LSWAX')
    const swax_supply = await getSupply( contracts.token_contract, 'SWAX')
    const g = await getDappGlobal()
    const r = await getRewardFarm()
    const cpu1_balance = await getDelBw('cpu1.fusion')
    const cpu2_balance = await getDelBw('cpu2.fusion')
    const cpu3_balance = await getDelBw('cpu3.fusion')
    const dapp_balances = await getBalances('dapp.fusion', contracts.wax_contract)
    const dapp_wax_balance = parseFloat(dapp_balances[0].balance)
    const self_staker = await getSWaxStaker('dapp.fusion')

    //the amounts contributing to dapp's expected wax balance
    const unpaid_rewards = parseFloat(r.rewardPool) - parseFloat(r.totalRewardsPaidOut)
    const claimed_rewards = parseFloat(g.total_rewards_claimed)
    const rental_pool = parseFloat(g.wax_available_for_rentals)
    const redemption_pool = parseFloat(g.wax_for_redemption)
    const revenue_pool = parseFloat(g.revenue_awaiting_distribution)
    let sum = unpaid_rewards - claimed_rewards + rental_pool + redemption_pool + revenue_pool;

    assert( dapp_wax_balance >= sum, `dapp has ${dapp_wax_balance} WAX but should have ${sum} WAX`)

    assert( Number(parseFloat(swax_supply.supply).toFixed(8)) == Number(parseFloat(g.swax_currently_earning) + parseFloat(g.swax_currently_backing_lswax)).toFixed(8), 
        `SWAX supply ${parseFloat(swax_supply.supply)} does not match ${parseFloat(g.swax_currently_earning) + parseFloat(g.swax_currently_backing_lswax)}` )

    assert( lswax_supply.supply == g.liquified_swax, `LSWAX supply of ${lswax_supply.supply} does not match dapp state of ${g.liquified_swax}` )

    assert(g.swax_currently_backing_lswax == self_staker.swax_balance, "self staker should match swax_backing_lswax")
}

/* Tests */


describe('\n\ncompound action', () => {

    it('error: hasnt been 5 minutes', async () => {
        const action = contracts.dapp_contract.actions.compound([]).send('dapp.fusion@active');
        await expectToThrow(action, "eosio_assert: must wait 5 minutes between compounds")
    });  

    it('success"', async () => {
        await incrementTime( (60*60*6) + 300)
        const state_before = await getDappGlobal()
        await contracts.dapp_contract.actions.compound([]).send('dapp.fusion@active');
        const state_after = await getDappGlobal()
        //1000 wax / (24*12) = expected_reward after 5 mins
        const expected_reward = parseFloat(initial_state.reward_pool) / (24 * 12)
        const expected_total = Number(parseFloat(state_before.swax_currently_backing_lswax) + expected_reward).toFixed(8);
        assert(expected_total == parseFloat(state_after.swax_currently_backing_lswax), `expected swax_currently_backing_lswax to be ${expected_total}`)
    });   

    it('success, then error if compounding twice in same second', async () => {
        await incrementTime( (60*60*6) + 300)
        const state_before = await getDappGlobal()
        await contracts.dapp_contract.actions.compound([]).send('dapp.fusion@active');
        const state_after = await getDappGlobal()
        //1000 wax / (24*12) = expected_reward after 5 mins
        const expected_reward = parseFloat(initial_state.reward_pool) / (24 * 12)
        const expected_total = Number(parseFloat(state_before.swax_currently_backing_lswax) + expected_reward).toFixed(8);
        assert(expected_total == parseFloat(state_after.swax_currently_backing_lswax), `expected swax_currently_backing_lswax to be ${expected_total}`)

        const action = contracts.dapp_contract.actions.compound([]).send('dapp.fusion@active');
        await expectToThrow(action, "eosio_assert: must wait 5 minutes between compounds")
    });           
});


describe('\n\naddadmin action', () => {

    it('error: missing auth of _self', async () => {
    	const action = contracts.dapp_contract.actions.addadmin(['mike']).send('mike@active');
    	await expectToThrow(action, "missing required authority dapp.fusion")
    });  

    it('error: admin_to_add is not a wax account', async () => {
        const action = contracts.dapp_contract.actions.addadmin(['john']).send('dapp.fusion@active');
        await expectToThrow(action, "eosio_assert: admin_to_add is not a wax account")
    });  

    it('error: oig is already an admin"', async () => {
        const action = contracts.dapp_contract.actions.addadmin(['oig']).send('dapp.fusion@active');
        await expectToThrow(action, "eosio_assert: oig is already an admin")
    });  

    it('success: added mike"', async () => {
        await contracts.dapp_contract.actions.addadmin(['mike']).send('dapp.fusion@active');
        const g = await getDappGlobal()
        assert( g.admin_wallets.indexOf('mike') > -1, "expected mike to be an admin" );
    });            
});


describe('\n\naddcpucntrct action', () => {

    it('error: missing auth of _self', async () => {
        const action = contracts.dapp_contract.actions.addcpucntrct(['mike']).send('mike@active');
        await expectToThrow(action, "missing required authority dapp.fusion")
    });  

    it('error: contract_to_add is not a wax account', async () => {
        const action = contracts.dapp_contract.actions.addcpucntrct(['john']).send('dapp.fusion@active');
        await expectToThrow(action, "eosio_assert: contract_to_add is not a wax account")
    });  

    it('error: cpu1.fusion is already an cpu contract"', async () => {
        const action = contracts.dapp_contract.actions.addcpucntrct(['cpu1.fusion']).send('dapp.fusion@active');
        await expectToThrow(action, "eosio_assert: cpu1.fusion is already a cpu contract")
    });  

    it('success: added mike"', async () => {
        await contracts.dapp_contract.actions.addcpucntrct(['mike']).send('dapp.fusion@active');
        const g = await getDappGlobal()
        assert( g.cpu_contracts.indexOf('mike') > -1, "expected mike to be a cpu contract" );
    });            
});


describe('\n\nclaimaslswax action', () => {

    it('error: missing auth of mike', async () => {
        const action = contracts.dapp_contract.actions.claimaslswax(['mike', lswax(1)]).send('eosio@active');
        await expectToThrow(action, "missing required authority mike")
    });  

    it('error: need to use the stake action first', async () => {
        const action = contracts.dapp_contract.actions.claimaslswax(['mike', lswax(1)]).send('mike@active');
        await expectToThrow(action, "eosio_assert: you don't have anything staked here")
    });  

    it('error: no rewards to claim', async () => {
        await stake('mike', 100)
        const action = contracts.dapp_contract.actions.claimaslswax(['mike', lswax(1)]).send('mike@active');
        await expectToThrow(action, "eosio_assert: you have no wax to claim")
    }); 

    it('error: output symbol mismatch', async () => {
        await stake('mike', 100)
        const action = contracts.dapp_contract.actions.claimaslswax(['mike', wax(1)]).send('mike@active');
        await expectToThrow(action, "eosio_assert: comparison of assets with different symbols is not allowed")
    }); 

    it('error: output amount must be positive', async () => {
        await stake('mike', 100)
        const action = contracts.dapp_contract.actions.claimaslswax(['mike', lswax(0)]).send('mike@active');
        await expectToThrow(action, "eosio_assert: Invalid output quantity.")
    });             

    it('success', async () => {
        await stake('bob', 1000)
        await simulate_days(10)
        await contracts.dapp_contract.actions.claimaslswax(['bob', lswax(0.15)]).send('bob@active');
    });   


    it('success without extend_reward', async () => {
        await setTime(1710482400)
        await stake('bob', 1000)

        const rewards_before = await getRewardFarm()
        const global_before = await getDappGlobal()
        const self_staker_before = await getSWaxStaker('dapp.fusion')
        const swax_supply_before = await getSupply(contracts.token_contract, 'SWAX')

        await incrementTime(600)

        const total_swax = parseFloat(global_before.swax_currently_earning) + parseFloat(global_before.swax_currently_backing_lswax)
        const bobs_percentage = 1000 / total_swax

        await contracts.dapp_contract.actions.claimaslswax(['bob', lswax(0.1)]).send('bob@active');        

        const rewards_after = await getRewardFarm()
        const global_after = await getDappGlobal()
        const self_staker_after = await getSWaxStaker('dapp.fusion')
        const swax_supply_after = await getSupply(contracts.token_contract, 'SWAX')

        // verify bob's lsWAX balance == 10 mins worth of rewards
        const expected_rewards = (1000 / 86400) * 600 * bobs_percentage;
        const bobs_balances = await getBalances('bob', contracts.token_contract)
        assert( parseFloat(bobs_balances[0]?.balance) == expected_rewards.toFixed(8), "rewards do not match expected amount" )

        // verify r total rewards paid out == 100% of 10 mins rewards
        const expected_total_paid_out = (1000 / 86400) * 600;
        assert( parseFloat(rewards_after?.totalRewardsPaidOut) == expected_total_paid_out.toFixed(8), "total paid out does not match expected amount" )

        // verify r totalSupply was increased by the claimed amount
        const claimed_amount = parseInt(expected_rewards.toFixed(8).toString().replace('.', ''), 10)
        const expected_total_supply = Number(rewards_before?.totalSupply) + claimed_amount
        assert( rewards_after?.totalSupply == expected_total_supply, "r.totalSupply is not the expected amount" )

        // verify staker.claimable_wax is 0 and staker.userRewardPerTokenPaid == r.rewardPerTokenStored
        const staker = await getSWaxStaker('bob')
        assert( staker?.claimable_wax == wax(0), "bob should have 0 wax to claim" )
        assert( staker?.userRewardPerTokenPaid == rewards_after?.rewardPerTokenStored, "userRewardPerTokenPaid != rewardPerTokenStored" )

        // verify self_staker.swax_balance was increased by claimed amount
        // claimable wax and userRewardPerTokenPaid should also be up to date
        const expected_claimable_wax = Number(expected_total_paid_out - expected_rewards).toFixed(8)
        const expected_swax_balance = Number(parseFloat(self_staker_before?.swax_balance) + expected_rewards).toFixed(8)
        assert( parseFloat(self_staker_after?.claimable_wax) == expected_claimable_wax, "self staker claimable_wax does not match expected amount" )
        assert( self_staker_after?.userRewardPerTokenPaid == rewards_after?.rewardPerTokenStored, "self_staker rewardPerTokenPaid does not match rewardPerTokenStored" )
        assert( parseFloat(self_staker_after?.swax_balance) == expected_swax_balance, "self_staker_after swax_balance does not match expected amount" )

        // verify g.swax_currently_backing_lswax, g.liquified_swax and lswax supply all increased by claimed amount
        // * this is actually all handled in `afterEach()`, since we know self_staker.swax_balance increased properly

        // verify swax supply increased by claimed amount
        const expected_swax_supply = parseFloat(swax_supply_before?.supply) + expected_rewards
        assert( swax_supply_after?.supply == swax(expected_swax_supply), "swax_supply_after does not match expected amount" )

        // verify swax_currently_earning is still 1000
        assert( global_after?.swax_currently_earning == global_before?.swax_currently_earning, "swax_currently_earning should not have changed" )

        // verify g.wax_available_for_rentals increased by claimed amount
        const expected_rental_bucket = wax(parseFloat(global_before.wax_available_for_rentals) + expected_rewards)
        assert( global_after?.wax_available_for_rentals == expected_rental_bucket, "wax_available_for_rentals does not match expected amount" )

        // verify g.total_rewards_claimed == claimed amount
        assert( global_after?.total_rewards_claimed == wax(expected_rewards), "total rewards claimed does not match claimed amount" )

    });  

});


describe('\n\nclaimgbmvote action', () => {

    it('error: not a cpu contract', async () => {
        const action = contracts.dapp_contract.actions.claimgbmvote(['mike']).send('mike@active');
        await expectToThrow(action, "eosio_assert: mike is not a cpu rental contract")
    });  

    it('error: not time to claim yet', async () => {
        await simulate_days(1)
        const action = contracts.dapp_contract.actions.claimgbmvote(['cpu1.fusion']).send('mike@active');
        await expectToThrow(action, "eosio_assert: hasnt been 24h since your last claim")
    });  

    it('success', async () => {
        await simulate_days(1, true, false)
        const rewards_before = await getDappGlobal();
        await contracts.dapp_contract.actions.claimgbmvote(['cpu1.fusion']).send('mike@active');
        const rewards_after = await getDappGlobal();
        assert( parseFloat(rewards_before.revenue_awaiting_distribution) < parseFloat(rewards_after.revenue_awaiting_distribution), "expected to have more rewards after claiming" )
    });  
});


describe('\n\nclaimrefunds action', () => {

    it('error: nothing to claim', async () => {
        const action = contracts.dapp_contract.actions.claimrefunds([]).send('mike@active');
        await expectToThrow(action, "eosio_assert: there are no refunds to claim")
    });  

    it('success', async () => {
        await simulate_days(11)
        await contracts.dapp_contract.actions.unstakecpu([initial_state.chain_time, 0]).send('mike@active');
        await incrementTime(86400 * 3)
        await contracts.dapp_contract.actions.claimrefunds([]).send('mike@active');
    });  
});


describe('\n\nclaimrewards action', () => {

    it('error: missing auth of staker', async () => {
        await stake('mike', 500)
        const action = contracts.dapp_contract.actions.claimrewards(['mike']).send('eosio@active');
        await expectToThrow(action, "missing required authority mike")
    });  
 
     it('error: not staking anything', async () => {
        const action = contracts.dapp_contract.actions.claimrewards(['mike']).send('mike@active');
        await expectToThrow(action, "eosio_assert: you don't have anything staked here")
    });

     it('error: nothing to claim', async () => {
        await stake('mike', 100)
        await simulate_days(7, false)
        await contracts.dapp_contract.actions.claimrewards(['mike']).send('mike@active');
        const action = contracts.dapp_contract.actions.claimrewards(['mike']).send('mike@active');
        await expectToThrow(action, "eosio_assert: you have no wax to claim")
    });      

     it('success', async () => {
        await stake('mike', 100)
        await simulate_days(7, false)
        const balance_before = await getBalances('mike', contracts.wax_contract)
        await contracts.dapp_contract.actions.stake(['bob']).send('bob@active');
        await contracts.dapp_contract.actions.stake(['ricky']).send('ricky@active');
        
        const vote_rewards = await getVoters()
    });  
     
});

describe('\n\nclearexpired action', () => {

    it('error: missing auth of user', async () => {
        const action = contracts.dapp_contract.actions.clearexpired(['mike']).send('eosio@active');
        await expectToThrow(action, "missing required authority mike")
    });  
 
    it('error: no expired requests', async () => {
        const action = contracts.dapp_contract.actions.clearexpired(['mike']).send('mike@active');
        await expectToThrow(action, "eosio_assert: there are no requests to clear")
    });  

    it('success', async () => {
        await simulate_days(7, true)
        await contracts.dapp_contract.actions.reqredeem(['mike', swax(100), true]).send('mike@active');
        await incrementTime(60*60*24*10)
        await contracts.dapp_contract.actions.clearexpired(['mike']).send('mike@active');
    });        
});


describe('\n\ncreatefarms action', () => {

    it('error: no incentives to distribute', async () => {
        const action = contracts.dapp_contract.actions.createfarms([]).send('mike@active');
        await expectToThrow(action, "eosio_assert: no lswax in the incentives_bucket")
    }); 

    it('success', async () => {
        await simulate_days(7, true)
        await contracts.dapp_contract.actions.createfarms([]).send('mike@active');
    });      

    it('error: hasnt been 7 days yet', async () => {
        await simulate_days(7, true)
        await contracts.dapp_contract.actions.createfarms([]).send('mike@active');
        const action = contracts.dapp_contract.actions.createfarms([]).send('mike@active');
        await expectToThrow(action, "eosio_assert: hasn't been 1 week since last farms were created");
    });  

    it('success with 2 farms', async () => {
        await contracts.dapp_contract.actions.setincentive([3, '4,HONEY', 'nfthivehoney', 1000000]).send('dapp.fusion@active');
        const incentives_after = await getDappIncentives()
        assert( incentives_after.length == 2, "there should be 2 incentives in the table" )
        await simulate_days(7, true)
        await incrementTime(86400*7)
        const dapp_state_before = await getDappGlobal()
        await contracts.dapp_contract.actions.createfarms([]).send('mike@active');
        const alcor_incentives = await getAlcorIncentives()
        assert( alcor_incentives.length == 2, "there should be 2 incentives on alcor" )
        const dapp_state_after = await getDappGlobal()
        almost_equal( parseFloat(dapp_state_before.incentives_bucket) * 0.74, parseFloat(dapp_state_after.incentives_bucket) )
    });  

    it('success with previous farms in table', async () => {
        await contracts.dapp_contract.actions.setincentive([3, '4,HONEY', 'nfthivehoney', 1000000]).send('dapp.fusion@active');
        const incentives_after = await getDappIncentives()
        assert( incentives_after.length == 2, "there should be 2 incentives in the table" )
        await simulate_days(7, true)
        await incrementTime(86400*7)
        const dapp_state_before = await getDappGlobal()
        await contracts.dapp_contract.actions.createfarms([]).send('mike@active');
        const alcor_incentives = await getAlcorIncentives()
        assert( alcor_incentives.length == 2, "there should be 2 incentives on alcor" )
        const dapp_state_after = await getDappGlobal()
        almost_equal( parseFloat(dapp_state_before.incentives_bucket) * 0.74, parseFloat(dapp_state_after.incentives_bucket) )

        await incrementTime(86400*7)
        const dapp_state_before_2 = await getDappGlobal()
        await contracts.dapp_contract.actions.createfarms([]).send('mike@active');
        const alcor_incentives_2 = await getAlcorIncentives()
        assert( alcor_incentives_2.length == 4, "there should be 4 incentives on alcor" )
        const dapp_state_after_2 = await getDappGlobal()
        almost_equal( parseFloat(dapp_state_before_2.incentives_bucket) * 0.74, parseFloat(dapp_state_after_2.incentives_bucket) )        
    });               
});


describe('\n\ninstaredeem action', () => {

    it('error: missing auth of user', async () => {
        const action = contracts.dapp_contract.actions.instaredeem(['eosio', swax(10)]).send('mike@active');
        await expectToThrow(action, "missing required authority eosio")
    }); 
      
    it('error: symbol mismatch', async () => {
        await stake('mike', 10)
        const action = contracts.dapp_contract.actions.instaredeem(['mike', lswax(10)]).send('mike@active');
        await expectToThrow(action, "eosio_assert: comparison of assets with different symbols is not allowed")
    });    

     it('error: not staking anything', async () => {
        const action = contracts.dapp_contract.actions.instaredeem(['mike', swax(10)]).send('mike@active');
        await expectToThrow(action, "eosio_assert: you don't have anything staked here")
    });  

     it('error: trying to redeem more than you have', async () => {
        await stake('mike', 10)
        const action = contracts.dapp_contract.actions.instaredeem(['mike', swax(11)]).send('mike@active');
        await expectToThrow(action, "eosio_assert: you are trying to redeem more than you have")
    });   

     it('error: must redeem positive quantity', async () => {
        await stake('mike', 10)
        const action = contracts.dapp_contract.actions.instaredeem(['mike', swax(0)]).send('mike@active');
        await expectToThrow(action, "eosio_assert: Must redeem a positive quantity")
    });

     it('error: not enough instaredeem funds available', async () => {
        await stake('mike', 1000)
        const memo = rent_cpu_memo('ricky', 49796.72299027, initial_state.chain_time)
        await contracts.wax_contract.actions.transfer(['ricky', 'dapp.fusion', wax(1000), memo]).send('ricky@active')        
        const action = contracts.dapp_contract.actions.instaredeem(['mike', swax(1000)]).send('mike@active');
        await expectToThrow(action, "eosio_assert: not enough instaredeem funds available")
    });

     it('success', async () => {
        await stake('mike', 10)    
        const bal_before = await getSWaxStaker('mike')
        assert(bal_before.swax_balance == swax(10), "expected 10 swax before")          
        await contracts.dapp_contract.actions.instaredeem(['mike', swax(10)]).send('mike@active');
        const bal_after = await getSWaxStaker('mike')
        assert(bal_after.swax_balance == swax(0), "expected 0 swax after")
    });     
});

describe('\n\nliquify action', () => {

    it('error: missing auth of user', async () => {
        const action = contracts.dapp_contract.actions.liquify(['eosio', swax(10)]).send('mike@active');
        await expectToThrow(action, "missing required authority eosio")
    }); 

     it('error: must liquify positive quantity', async () => {
        await stake('mike', 10)
        const action = contracts.dapp_contract.actions.liquify(['mike', swax(0)]).send('mike@active');
        await expectToThrow(action, "eosio_assert: Invalid quantity.")
    });     
      
    it('error: symbol mismatch', async () => {
        const action = contracts.dapp_contract.actions.liquify(['mike', lswax(10)]).send('mike@active');
        await expectToThrow(action, "eosio_assert: comparison of assets with different symbols is not allowed")
    });    

     it('error: not staking anything', async () => {
        const action = contracts.dapp_contract.actions.liquify(['mike', swax(10)]).send('mike@active');
        await expectToThrow(action, "eosio_assert: you don't have anything staked here")
    });  

     it('error: trying to liquify more than you have', async () => {
        await stake('mike', 10)
        const action = contracts.dapp_contract.actions.liquify(['mike', swax(11)]).send('mike@active');
        await expectToThrow(action, "eosio_assert: you are trying to liquify more than you have")
    });   
  
     it('success', async () => {
        await stake('mike', 10)
        const bal_before = await getBalances('mike', contracts.token_contract)
        assert(bal_before.length == 0, "expected no lsWAX balance")
        await contracts.dapp_contract.actions.liquify(['mike', swax(10)]).send('mike@active');
        const bal_after = await getBalances('mike', contracts.token_contract)
        assert(bal_after[0].balance == lswax(10), "expected balance after to be 10 lsWAX")        
    });    

});

describe('\n\nliquifyexact action', () => {

    it('error: missing auth of user', async () => {
        const action = contracts.dapp_contract.actions.liquifyexact(['eosio', swax(10), lswax(10)]).send('mike@active');
        await expectToThrow(action, "missing required authority eosio")
    }); 

    it('error: must liquify positive quantity', async () => {
        await stake('mike', 10)
        const action = contracts.dapp_contract.actions.liquifyexact(['mike', swax(0), lswax(10)]).send('mike@active');
        await expectToThrow(action, "eosio_assert: Invalid quantity.")
    });     
      
    it('error: output quantity not positive', async () => {
        await stake('mike', 10)
        const action = contracts.dapp_contract.actions.liquifyexact(['mike', swax(10), lswax(0)]).send('mike@active');
        await expectToThrow(action, "eosio_assert: Invalid output quantity.")
    });       

    it('error: input symbol mismatch', async () => {
        const action = contracts.dapp_contract.actions.liquifyexact(['mike', lswax(10), lswax(10)]).send('mike@active');
        await expectToThrow(action, "eosio_assert: comparison of assets with different symbols is not allowed")
    });    

    it('error: output symbol mismatch', async () => {
        const action = contracts.dapp_contract.actions.liquifyexact(['mike', swax(10), swax(10)]).send('mike@active');
        await expectToThrow(action, "eosio_assert: comparison of assets with different symbols is not allowed")
    });      

    it('error: not staking anything', async () => {
        const action = contracts.dapp_contract.actions.liquifyexact(['mike', swax(10), lswax(10)]).send('mike@active');
        await expectToThrow(action, "eosio_assert: you don't have anything staked here")
    });  
     
    it('error: trying to liquify more than you have', async () => {
        await stake('mike', 10)
        const action = contracts.dapp_contract.actions.liquifyexact(['mike', swax(11), lswax(10)]).send('mike@active');
        await expectToThrow(action, "eosio_assert: you are trying to liquify more than you have")
    });   
  
    it('error: output less than expected', async () => {
        await stake('mike', 10)
        const action = contracts.dapp_contract.actions.liquifyexact(['mike', swax(10), lswax(20)]).send('mike@active');
        const error_message = `eosio_assert_message: output would be ${lswax(10)} but expected ${lswax(20)}`
        await expectToThrow(action, error_message)
    });  

     it('success', async () => {
        await stake('mike', 10)
        const bal_before = await getBalances('mike', contracts.token_contract)
        assert(bal_before.length == 0, "expected no lsWAX balance")
        await contracts.dapp_contract.actions.liquifyexact(['mike', swax(10), lswax(10)]).send('mike@active');
        const bal_after = await getBalances('mike', contracts.token_contract)
        assert(bal_after[0].balance == lswax(10), "expected balance after to be 10 lsWAX")        
    });  

});

describe('\n\nreallocate action', () => {

    it('error: redemption period has not ended yet', async () => {
        const action = contracts.dapp_contract.actions.reallocate([]).send('mike@active');
        await expectToThrow(action, "eosio_assert: redemption period has not ended yet")
    }); 
 
    it('error: there is no wax to reallocate', async () => {
        await incrementTime( (60*60*24*9) + 1)
        const action = contracts.dapp_contract.actions.reallocate([]).send('mike@active');
        await expectToThrow(action, "eosio_assert: there is no wax to reallocate")
    }); 

    it('success', async () => {
        await stake('mike', 10000)
        await incrementTime(86400)
        await contracts.dapp_contract.actions.stakeallcpu([]).send('mike@active');
        await contracts.dapp_contract.actions.reqredeem(['mike', swax(10000), true]).send('mike@active');
        await incrementTime(86400*10)
        await contracts.dapp_contract.actions.sync(['dapp.fusion']).send('dapp.fusion@active')
        await incrementTime(86400*7)
        await contracts.dapp_contract.actions.unstakecpu([initial_state.chain_time + (86400*7), 500]).send('mike@active');
        await incrementTime( (86400*3) )
        await contracts.dapp_contract.actions.claimrefunds([]).send('mike@active');
        const dapp_state_before = await getDappGlobal()
        await incrementTime( (60*60*24*2) + 1)
        await contracts.dapp_contract.actions.reallocate([]).send('mike@active');
        const dapp_state_after = await getDappGlobal()
        assert( parseFloat(dapp_state_after.wax_for_redemption) + 10000 == parseFloat(dapp_state_before.wax_for_redemption), "expected 10,000 less redemption wax after reallocation" )
        assert( parseFloat(dapp_state_after.wax_available_for_rentals) - 10000 == parseFloat(dapp_state_before.wax_available_for_rentals), "expected 10,000 more rental wax after reallocation" )
    }); 
    
});

describe('\n\nredeem action', () => {

    it('error: missing auth of user', async () => {
        const action = contracts.dapp_contract.actions.redeem(['mike']).send('eosio@active');
        await expectToThrow(action, "missing required authority mike")
    }); 
 
    it('error: not staking anything', async () => {
        const action = contracts.dapp_contract.actions.redeem(['mike']).send('mike@active');
        await expectToThrow(action, "eosio_assert: you don't have anything staked here")
    }); 

    it('error: not redemption time yet', async () => {
        await stake('mike', 10)
        await incrementTime(86400*3)
        const action = contracts.dapp_contract.actions.redeem(['mike']).send('mike@active');
        await expectToThrow(action, `eosio_assert: next redemption does not start until ${initial_state.chain_time + (60*60*24*7)}`)        
    });

    it('error: no pending requests', async () => {
        await stake('mike', 10)
        const action = contracts.dapp_contract.actions.redeem(['mike']).send('mike@active');
        await expectToThrow(action, `eosio_assert: you don't have a redemption request for the current redemption period`)        
    }); 
    
    it('success', async () => {
        await stake('mike', 10)
        await incrementTime(86400)
        await contracts.dapp_contract.actions.stakeallcpu([]).send('mike@active');
        await contracts.dapp_contract.actions.reqredeem(['mike', swax(10), true]).send('mike@active');
        await incrementTime(60*60*24*17)
        await contracts.dapp_contract.actions.unstakecpu([initial_state.chain_time + (60*60*24*7), 0]).send('mike@active');
        await incrementTime(60*60*24*3)
        await contracts.dapp_contract.actions.claimrefunds([]).send('mike@active');             
        const dapp_state_before = await getDappGlobal()
        assert(dapp_state_before.wax_for_redemption == wax(10), "expected 10 wax awaiting redemption")
        await contracts.dapp_contract.actions.redeem(['mike']).send('mike@active');
        const dapp_state_after = await getDappGlobal()
        assert(dapp_state_after.wax_for_redemption == wax(0), "expected 0 wax awaiting redemption")      
    });
    
});


describe('\n\nremoveadmin action', () => {

    it('error: missing auth of self', async () => {
        const action = contracts.dapp_contract.actions.removeadmin(['mike']).send('eosio@active');
        await expectToThrow(action, "missing required authority dapp.fusion")
    }); 
 
     it('error: mike is not an admin', async () => {
        const action = contracts.dapp_contract.actions.removeadmin(['mike']).send('dapp.fusion@active');
        await expectToThrow(action, "eosio_assert: mike is not an admin")
    }); 
    
     it('success', async () => {
        await contracts.dapp_contract.actions.removeadmin(['oig']).send('dapp.fusion@active');
        const dapp_config_after = await getDappGlobal()
        assert( dapp_config_after.admin_wallets.indexOf('oig') == -1, "expected oig to not be an admin" )
    });     
});

describe('\n\nreqredeem action', () => {

    it('error: you have a pending request but passed false for replacing it', async () => {
        await stake('mike', 10)
        await incrementTime(86400)
        await contracts.dapp_contract.actions.stakeallcpu([]).send('mike@active');
        await contracts.dapp_contract.actions.reqredeem(['mike', swax(10), true]).send('mike@active');
        const action = contracts.dapp_contract.actions.reqredeem(['mike', swax(10), false]).send('mike@active');
        await expectToThrow(action, `eosio_assert: you have previous requests but passed 'false' to the accept_replacing_prev_requests param`)
    }); 
 
});

describe('\n\nrmvcpucntrct action', () => {

    it('error: missing auth of self', async () => {
        const action = contracts.dapp_contract.actions.rmvcpucntrct(['mike']).send('eosio@active');
        await expectToThrow(action, "missing required authority dapp.fusion")
    }); 
 
     it('error: mike is not a cpu contract', async () => {
        const action = contracts.dapp_contract.actions.rmvcpucntrct(['mike']).send('dapp.fusion@active');
        await expectToThrow(action, "eosio_assert: mike is not a cpu contract")
    });

     it('success', async () => {
        await contracts.dapp_contract.actions.rmvcpucntrct(['cpu2.fusion']).send('dapp.fusion@active');
        const dapp_config_after = await getDappGlobal()
        assert( dapp_config_after.cpu_contracts.indexOf('cpu2.fusion') == -1, "expected cpu2.fusion to not be in the config" )        
    });     
});


describe('\n\nrmvincentive action', () => {

    it('error: missing auth of self', async () => {
        const action = contracts.dapp_contract.actions.rmvincentive([2]).send('eosio@active');
        await expectToThrow(action, "missing required authority dapp.fusion")
    }); 
     
    it(`error: this poolId doesn't exist in the lpfarms table`, async () => {
        const action = contracts.dapp_contract.actions.rmvincentive([1]).send('dapp.fusion@active');
        await expectToThrow(action, `eosio_assert: this poolId doesn't exist in the lpfarms table`)
    });   

    it(`success`, async () => {
        const incentives_before = await getDappIncentives()
        assert( incentives_before.length == 1, "there should be 1 incentive in the table" )
        await contracts.dapp_contract.actions.rmvincentive([2]).send('dapp.fusion@active');
        const incentives_after = await getDappIncentives()
        assert( incentives_after.length == 0, "there should be 0 incentives in the table" )        
    });         
});

describe('\n\nsetfallback action', () => {

    it('error: missing auth of caller', async () => {
        const action = contracts.dapp_contract.actions.setfallback(['mike', 'mike']).send('eosio@active');
        await expectToThrow(action, "missing required authority mike")
    }); 
     
    it(`error: caller is not an admin`, async () => {
        const action = contracts.dapp_contract.actions.setfallback(['mike', 'mike']).send('mike@active');
        await expectToThrow(action, "eosio_assert: this action requires auth from one of the admin_wallets in the global table")
    });   

    it(`error: cpu receiver is not a wax account`, async () => {
        const action = contracts.dapp_contract.actions.setfallback(['dapp.fusion', 'someguy']).send('dapp.fusion@active');
        await expectToThrow(action, "eosio_assert: cpu receiver is not a wax account")
    });     

    it(`success`, async () => {
        const config_before = await getDappGlobal()
        assert( config_before.fallback_cpu_receiver == 'updatethings', "expected updatethings to be the fallback" )
        await contracts.dapp_contract.actions.setfallback(['dapp.fusion', 'mike']).send('dapp.fusion@active');
        const config_after = await getDappGlobal()
        assert( config_after.fallback_cpu_receiver == 'mike', "expected mike to be the fallback" )
    });         
});

describe('\n\nsetincentive action', () => {

    it('error: missing auth of self', async () => {
        const action = contracts.dapp_contract.actions.setincentive([3, '4,HONEY', 'nfthivehoney', 1000000]).send('eosio@active');
        await expectToThrow(action, "missing required authority dapp.fusion")        
    }); 
     
    it('error: share must be positive', async () => {
        const action = contracts.dapp_contract.actions.setincentive([3, '4,HONEY', 'nfthivehoney', 0]).send('dapp.fusion@active');
        await expectToThrow(action, `eosio_assert: percent_share_1e6 must be positive`)        
    });   

    it('error: pool does not exist', async () => {
        const action = contracts.dapp_contract.actions.setincentive([4, '4,HONEY', 'nfthivehoney', 1000000]).send('dapp.fusion@active');
        await expectToThrow(action, `eosio_assert: this poolId does not exist`)        
    });  

    it('error: pool does not contain the token entered', async () => {
        const action = contracts.dapp_contract.actions.setincentive([2, '4,HONEY', 'nfthivehoney', 1000000]).send('dapp.fusion@active');
        await expectToThrow(action, `eosio_assert: this poolId does not contain the symbol/contract combo you entered`)        
    });   

    it('error: tokenA should be lsWAX', async () => {
        const action = contracts.dapp_contract.actions.setincentive([2, '8,LSWAX', 'token.fusion', 1000000]).send('dapp.fusion@active');
        await expectToThrow(action, `eosio_assert: tokenA should be lsWAX`)        
    });  

    it('success', async () => {
        await contracts.dapp_contract.actions.setincentive([3, '4,HONEY', 'nfthivehoney', 1000000]).send('dapp.fusion@active');
        const incentives_after = await getDappIncentives()
        assert( incentives_after.length == 2, "there should be 2 incentives in the table" )
    });                          
});

describe('\n\nsetpolshare action', () => {

    it('error: missing auth of self', async () => {
        const action = contracts.dapp_contract.actions.setpolshare([5000000]).send('eosio@active');
        await expectToThrow(action, "missing required authority dapp.fusion")        
    }); 
         
    it('error: share must be >= 5%', async () => {
        const action = contracts.dapp_contract.actions.setpolshare([4900000]).send('dapp.fusion@active');
        await expectToThrow(action, `eosio_assert: acceptable range is 5-10%`)        
    });   

    it('error: share must be <= 10%', async () => {
        const action = contracts.dapp_contract.actions.setpolshare([10000001]).send('dapp.fusion@active');
        await expectToThrow(action, `eosio_assert: acceptable range is 5-10%`)        
    });   

    it('success', async () => {
        await contracts.dapp_contract.actions.setpolshare([6942000]).send('dapp.fusion@active');
        const dapp_config = await getDappGlobal()
        assert( dapp_config.pol_share_1e6 == 6942000, "pol share should be 6942000" )
    });                                       
});

describe('\n\nsetrentprice action', () => {

    it('error: missing auth of caller', async () => {
        const action = contracts.dapp_contract.actions.setrentprice(['mike', wax(0.2)]).send('eosio@active');
        await expectToThrow(action, "missing required authority mike")        
    }); 
     
    it('error: caller is not an admin', async () => {
        const action = contracts.dapp_contract.actions.setrentprice(['mike', wax(0.2)]).send('mike@active');
        await expectToThrow(action, "eosio_assert: this action requires auth from one of the admin_wallets in the config table")        
    });                                             

    it('error: cost must be positive', async () => {
        const action = contracts.dapp_contract.actions.setrentprice(['oig', wax(0)]).send('oig@active');
        await expectToThrow(action, "eosio_assert: cost must be positive")        
    }); 

    it('error: symbol mismatch', async () => {
        const action = contracts.dapp_contract.actions.setrentprice(['oig', lswax(0.1)]).send('oig@active');
        await expectToThrow(action, "eosio_assert: comparison of assets with different symbols is not allowed")        
    });         

    it('success', async () => {
        await contracts.dapp_contract.actions.setrentprice(['oig', wax(0.42)]).send('oig@active');
        const dapp_state = await getDappGlobal()
        assert( dapp_state.cost_to_rent_1_wax == wax(0.42), "rent price should be 0.42 wax" )    
    });    
});

describe('\n\nstake action', () => {

    it('error: missing auth of staker', async () => {
        const action = contracts.dapp_contract.actions.stake(['mike']).send('eosio@active');
        await expectToThrow(action, "missing required authority mike")        
    }); 

});

describe('\n\nstakeallcpu action', () => {

    it('error: next stakeall time is > now', async () => {
        const action = contracts.dapp_contract.actions.stakeallcpu([]).send('mike@active');
        await expectToThrow(action, "eosio_assert: next stakeall time is not until 1710547200")
    }); 

    it('success', async () => {
        await incrementTime(86400)
        await contracts.dapp_contract.actions.stakeallcpu([]).send('mike@active');
    }); 
});

describe('\n\nunstakecpu action', () => {

    it('error: epoch doesnt exist', async () => {
        const action = contracts.dapp_contract.actions.unstakecpu([initial_state.chain_time - 1, 0]).send('mike@active');
        await expectToThrow(action, `eosio_assert: could not find epoch ${initial_state.chain_time -1}`)
    }); 

    it('error: not time to unstake yet', async () => {
        const action = contracts.dapp_contract.actions.unstakecpu([initial_state.chain_time, 0]).send('mike@active');
        await expectToThrow(action, `eosio_assert: can not unstake until another ${(86400*11)} seconds has passed`)
    }); 

    it('error: nothing to unstake', async () => {
        await incrementTime(86400*11)
        const action = contracts.dapp_contract.actions.unstakecpu([initial_state.chain_time, 0]).send('mike@active');
        await expectToThrow(action, `eosio_assert: cpu1.fusion has nothing to unstake`)
    });    

    it('success', async () => {
        await simulate_days(2)
        await incrementTime(86400*11)
        await contracts.dapp_contract.actions.unstakecpu([initial_state.chain_time, 0]).send('mike@active');
    });       
});


describe('\n\nsync action', () => {

    it('error: missing auth of caller', async () => {
        const action = contracts.dapp_contract.actions.sync(['mike']).send('eosio@active');
        await expectToThrow(action, `missing required authority mike`)
    }); 
      
    it('error: caller is not an admin', async () => {
        const action = contracts.dapp_contract.actions.sync(['mike']).send('mike@active');
        await expectToThrow(action, `eosio_assert: mike is not an admin`)
    });

    it('success', async () => {
        await contracts.dapp_contract.actions.sync(['oig']).send('oig@active');
    });           
});

describe('\n\nupdatetop21 action', () => {

    it('error: hasnt been 24h', async () => {
        const action = contracts.dapp_contract.actions.updatetop21([]).send('mike@active');
        await expectToThrow(action, `eosio_assert: hasn't been 24h since last top21 update`)
    }); 

    //this fails in unit testing environment because of secondary indexing, see the comment
    //in fusion.entry.cpp under the `inittop21` action
    it('error: not voting for enough producers', async () => {
        await incrementTime(86400)
        const action = contracts.dapp_contract.actions.updatetop21([]).send('mike@active');
        await expectToThrow(action, `eosio_assert: attempting to vote for 1 producers but need to vote for 16`)
    });       
});


describe('\n\n0 distribution', () => {

    it('distribute 1 day, then again with 0 revenue', async () => {
        await incrementTime(60*60*6)
        await stake('mike', 10)
        await incrementTime(86400)
        await contracts.dapp_contract.actions.compound([]).send('dapp.fusion@active');
        await incrementTime(300)
        await contracts.dapp_contract.actions.claimrewards(['mike']).send('mike@active');
        await incrementTime(86400)
        const action1 = contracts.dapp_contract.actions.claimrewards(['mike']).send('mike@active');
        await expectToThrow(action1, "eosio_assert: you have no wax to claim")
        const action2 = contracts.dapp_contract.actions.compound([]).send('dapp.fusion@active');
        await expectToThrow(action2, "eosio_assert: nothing to compound")
    });           

});

describe('\n\nextend_reward', () => {

    it('test skipping multiple days before extend_farm is called', async () => {
        await incrementTime(60*60*6)
        await stake('mike', 10000)
        await contracts.wax_contract.actions.transfer(['eosio', 'dapp.fusion', wax(10), 'waxfusion_revenue']).send('eosio@active')
        await incrementTime(86400*7)
        
        await contracts.dapp_contract.actions.stake(['mike']).send('mike@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'dapp.fusion', wax(10), 'waxfusion_revenue']).send('eosio@active')
        await incrementTime(86400)
  
        await stake('mike', 10000)
        await contracts.wax_contract.actions.transfer(['eosio', 'dapp.fusion', wax(10), 'waxfusion_revenue']).send('eosio@active')
        await incrementTime(86400)

        await stake('mike', 10000)
        await contracts.wax_contract.actions.transfer(['eosio', 'dapp.fusion', wax(10), 'waxfusion_revenue']).send('eosio@active')
        await incrementTime(86400)

        await stake('mike', 10000)
        await contracts.wax_contract.actions.transfer(['eosio', 'dapp.fusion', wax(10), 'waxfusion_revenue']).send('eosio@active')
        await incrementTime(86400);

        await contracts.dapp_contract.actions.stake(['mike']).send('mike@active');                           
        await contracts.dapp_contract.actions.compound([]).send('dapp.fusion@active');  
        await incrementTime(86400*10)
        await contracts.dapp_contract.actions.claimrewards(['mike']).send('mike@active');                           
        await contracts.dapp_contract.actions.compound([]).send('dapp.fusion@active'); 

        //total paid out/claimed should be initial + (50 * 0.85), 0 awaiting distribution
        const r = await getRewardFarm()
        const g = await getDappGlobal()   
        const expected_payouts = parseFloat(initial_state.reward_pool) + ( 50 * 0.85 )
        assert( parseFloat(r.rewardPool) == expected_payouts, `reward pool should be ${expected_payouts}` )
        almost_equal( parseFloat(r.totalRewardsPaidOut), expected_payouts )     
        assert( g.revenue_awaiting_distribution == wax(0), `there should be 0 wax awaiting distribution` )
    });           

    
    it('simulate days and stake users to auto trigger extend_reward', async () => {
        await incrementTime(60*60*6)
        await stake('mike', 10000)
        await contracts.wax_contract.actions.transfer(['eosio', 'dapp.fusion', wax(10), 'waxfusion_revenue']).send('eosio@active')
        await incrementTime(86400*7)
        await contracts.dapp_contract.actions.stake(['mike']).send('mike@active');

        await contracts.dapp_contract.actions.stake(['mike']).send('mike@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'dapp.fusion', wax(10), 'waxfusion_revenue']).send('eosio@active')
        await incrementTime(86400)        

        await contracts.dapp_contract.actions.compound([]).send('dapp.fusion@active');
        await contracts.dapp_contract.actions.stake(['mike']).send('mike@active');  

        //there should be 1000 + 8.5 wax paid out, and 10 wax awaiting distribution
        const r = await getRewardFarm()
        const g = await getDappGlobal()
        const expected_payouts = parseFloat(initial_state.reward_pool) + ( 10 * 0.85 )
        assert( parseFloat(r.rewardPool) == expected_payouts, `reward pool should be ${expected_payouts} but is ${parseFloat(r.rewardPool)}` )
        almost_equal( parseFloat(r.totalRewardsPaidOut), expected_payouts )      
        assert( g.revenue_awaiting_distribution == wax(10), `there should be 0 wax awaiting distribution` )     
    }); 
       
});
