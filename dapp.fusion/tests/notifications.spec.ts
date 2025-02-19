const { blockchain, contracts, incrementTime, init, initial_state, setTime, stake, simulate_days, unliquify } = require("./setup.spec.ts")
const { almost_equal, calculate_wax_and_lswax_outputs, honey, lswax, rent_cpu_memo, swax, wax } = require("./helpers.ts")
const { nameToBigInt, TimePoint, expectToThrow } = require("@eosnetwork/vert");
const { Asset, Int64, Name, UInt64, UInt128, TimePointSec } = require('@wharfkit/antelope');
const { assert } = require("chai");

const [mike, bob, ricky] = blockchain.createAccounts('mike', 'bob', 'ricky')

const scopes = {
    alcor: contracts.alcor_contract.value,
    dapp: contracts.dapp_contract.value,
    cpu1: contracts.cpu1.value,
    cpu2: contracts.cpu2.value,
    cpu3: contracts.cpu3.value,
    pol: contracts.pol_contract.value,
    system: contracts.system_contract.value
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

const getAllAlcorPools = async (log = false) => {
    const alcor_pools = await contracts.alcor_contract.tables
        .pools(scopes.alcor)
        .getTableRows()
    if(log){
        console.log("all alcor pools:")
        console.log(alcor_pools) 
    }  
    return alcor_pools
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

const getDappGlobal2 = async (log = false) => {
    const g2 = await contracts.dapp_contract.tables
        .global2(scopes.dapp)
        .getTableRows()[0]
    if(log){
        console.log('global2:')
        console.log(g2)
    }
    return g2
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

const getEcosystemFund = async (log = false) => {
    const lpfarms = await contracts.dapp_contract.tables
        .lpfarms(scopes.dapp)
        .getTableRows()
    if(log){
        console.log('lpfarms:')
        console.log(lpfarms)  
    }
    return lpfarms    
}

const getIncentiveIdsTable = async (log = false) => {
    const incentiveids = await contracts.dapp_contract.tables
        .incentiveids(scopes.dapp)
        .getTableRows()
    if(log){
        console.log('incentiveids:')
        console.log(incentiveids)  
    }
    return incentiveids    
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


describe('\n\nstake memo', () => {

    it('error: need to use the stake action first', async () => {
    	const action = contracts.wax_contract.actions.transfer(['mike', 'dapp.fusion', wax(10), 'stake']).send('mike@active');
    	await expectToThrow(action, "eosio_assert: you don't have anything staked here")
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
    	const dapp_state = await getDappGlobal();
    	assert(dapp_state.swax_currently_earning == swax(10), "swax_currently_earning should be 10");
        almost_equal( parseFloat(dapp_state.wax_available_for_rentals), parseFloat(initial_state.dapp_rental_pool) + 10  );
    	const swax_supply = await getSupply(contracts.token_contract, "SWAX");
        almost_equal( parseFloat(swax_supply.supply), parseFloat(initial_state.dapp_rental_pool) + 10  );
    });     
});


describe('\n\nunliquify memo', () => {

    it('error: need to use the stake action first', async () => {
        await stake('mike', 100, true)
        await contracts.token_contract.actions.transfer(['mike', 'ricky', lswax(10), '']).send('mike@active');
        const action = contracts.token_contract.actions.transfer(['ricky', 'dapp.fusion', lswax(10), 'unliquify']).send('ricky@active');
        await expectToThrow(action, "eosio_assert: you don't have anything staked here")
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
        const dapp_state = await getDappGlobal()
        assert(dapp_state.revenue_awaiting_distribution == wax(100000), "dapp revenue should be 100k wax")
    }); 
});


describe('\n\nlp_incentives memo', () => {

    it('success, 100k wax deposited', async () => {
        await contracts.wax_contract.actions.transfer(['mike', 'dapp.fusion', wax(100000), 'lp_incentives']).send('mike@active');
        const dapp_state2 = await getDappGlobal()
        assert(dapp_state2.incentives_bucket == lswax(100000), "incentives bucket should be 100k lswax")
    }); 

    it('success, 1k lsWAX deposited', async () => {
        await stake('mike', 1000, true)
        await contracts.token_contract.actions.transfer(['mike', 'dapp.fusion', lswax(1000), 'lp_incentives']).send('mike@active');
        const dapp_state2 = await getDappGlobal()
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
 
    it('error: !is_account( receiver )', async () => {
        const memo = rent_cpu_memo('steve', 100, initial_state.chain_time)
        const action = contracts.wax_contract.actions.transfer(['mike', 'dapp.fusion', wax(10), memo]).send('mike@active') 
        await expectToThrow(action, "eosio_assert: steve is not an account")
    });  

    it('error: incomplete memo', async () => {
        const action = contracts.wax_contract.actions.transfer(['mike', 'dapp.fusion', wax(10), `|rent_cpu|mike|10|`]).send('mike@active') 
        await expectToThrow(action, "eosio_assert: memo for rent_cpu operation is incomplete")
    });     

    it('error: max amount to rent', async () => {
        const memo = rent_cpu_memo('mike', 10000001, initial_state.chain_time)
        const action = contracts.wax_contract.actions.transfer(['mike', 'dapp.fusion', wax(10), memo]).send('mike@active') 
        await expectToThrow(action, "eosio_assert: maximum wax amount to rent is 10000000")
    }); 

    it('error: min amount to rent', async () => {
        const memo = rent_cpu_memo('mike', 9, initial_state.chain_time)
        const action = contracts.wax_contract.actions.transfer(['mike', 'dapp.fusion', wax(10), memo]).send('mike@active') 
        await expectToThrow(action, "eosio_assert: minimum wax amount to rent is 10")
    }); 

    it('error: not enough wax in rental pool', async () => {
        const memo = rent_cpu_memo('mike', 10000000, initial_state.chain_time)
        const action = contracts.wax_contract.actions.transfer(['mike', 'dapp.fusion', wax(10), memo]).send('mike@active') 
        await expectToThrow(action, "eosio_assert: there is not enough wax in the rental pool to cover this rental")
    }); 
    
    it('error: too late for this epoch', async () => {
        await incrementTime(86400*11)
        const memo = rent_cpu_memo('mike', 10, initial_state.chain_time)
        const action = contracts.wax_contract.actions.transfer(['mike', 'dapp.fusion', wax(10), memo]).send('mike@active') 
        await expectToThrow(action, "eosio_assert: it is too late to rent from this epoch, please rent from the next one")
    });  

    it('error: epoch does not exist', async () => {
        const memo = rent_cpu_memo('mike', 10, initial_state.chain_time - (86400*7))
        const action = contracts.wax_contract.actions.transfer(['mike', 'dapp.fusion', wax(10), memo]).send('mike@active') 
        await expectToThrow(action, `eosio_assert: epoch ${initial_state.chain_time - (86400*7)} does not exist`)
    });      

    it('error: didnt send enough wax', async () => {
        const memo = rent_cpu_memo('mike', 1000, initial_state.chain_time)
        const action = contracts.wax_contract.actions.transfer(['mike', 'dapp.fusion', wax(1), memo]).send('mike@active') 
        await expectToThrow(action, `eosio_assert: expected to receive ${wax(13.2)}`)
    });   

    it('success', async () => {
        const memo = rent_cpu_memo('mike', 1000, initial_state.chain_time)
        await contracts.wax_contract.actions.transfer(['mike', 'dapp.fusion', wax(13.2), memo]).send('mike@active') 
    });            
});


describe('\n\nunliquify_exact memo', () => {
    const memo = `|unliquify_exact|1000000000|`

    it('error: only lswax accepted', async () => {
        await stake('mike', 1000, true)
        const action = contracts.wax_contract.actions.transfer(['mike', 'dapp.fusion', wax(10), memo]).send('mike@active') 
        await expectToThrow(action, "eosio_assert: only LSWAX can be unliquified")        
    });  
 
     it('error: incomplete memo', async () => {
        await stake('mike', 1000, true)
        const action = contracts.token_contract.actions.transfer(['mike', 'dapp.fusion', lswax(10), `|unliquify_exact|`]).send('mike@active') 
        await expectToThrow(action, "eosio_assert: memo for unliquify_exact operation is incomplete")        
    });  

    it('error: minimum not met', async () => {
        await stake('mike', 1000, true)
        const action = contracts.token_contract.actions.transfer(['mike', 'dapp.fusion', lswax(0.9), memo]).send('mike@active') 
        await expectToThrow(action, "eosio_assert: minimum unliquify amount not met")        
    });   

    it('error: need to use the stake action first', async () => {
        await stake('mike', 100, true)
        await contracts.token_contract.actions.transfer(['mike', 'ricky', lswax(10), '']).send('mike@active');
        const action = contracts.token_contract.actions.transfer(['ricky', 'dapp.fusion', lswax(10), memo]).send('ricky@active');
        await expectToThrow(action, "eosio_assert: you don't have anything staked here")
    });  

    it('error: output less than expected', async () => {
        await stake('mike', 1000, true)
        const action = contracts.token_contract.actions.transfer(['mike', 'dapp.fusion', lswax(10), '|unliquify_exact|1100000000|']).send('mike@active') 
        await expectToThrow(action, "eosio_assert_message: output would be 10.00000000 SWAX but expected 11.00000000 SWAX")        
    });    

    it('error: minimum out of range', async () => {
        await stake('mike', 1000, true)
        const action = contracts.token_contract.actions.transfer(['mike', 'dapp.fusion', lswax(10), '|unliquify_exact|4611686018427387904|']).send('mike@active') 
        await expectToThrow(action, "eosio_assert: minimum_output is out of range")        
    });   

    it('success', async () => {
        await stake('mike', 1000, true)
        await contracts.token_contract.actions.transfer(['mike', 'dapp.fusion', lswax(10), memo]).send('mike@active') 
    });                  
});

describe('\n\nsimulate_days', () => {

    it('success', async () => {
        await simulate_days(14, true)

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

    });  
 
});


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

describe('\n\ninstant redeem memo', () => {
    
    it('error: only lswax accepted', async () => {
        const action = contracts.wax_contract.actions.transfer(['mike', 'dapp.fusion', wax(10), 'instant redeem']).send('mike@active');
        await expectToThrow(action, `eosio_assert: only LSWAX should be sent with this memo`)
    });  

    it('error: sender should be pol.fusion', async () => {
        await stake('mike', 10, true)
        const action = contracts.token_contract.actions.transfer(['mike', 'dapp.fusion', lswax(10), 'instant redeem']).send('mike@active');
        await expectToThrow(action, `eosio_assert: expected pol.fusion to be the sender`)
    }); 

    it('error: minimum not met', async () => {
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(10), '']).send('mike@active');
        await stake('pol.fusion', 10, true)
        const action = contracts.token_contract.actions.transfer(['pol.fusion', 'dapp.fusion', lswax(0.9), 'instant redeem']).send('pol.fusion@active');
        await expectToThrow(action, `eosio_assert: minimum unliquify amount not met`)
    });      
  
    it('error: not enough instant redeem funds available', async () => {
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(100000), '']).send('mike@active');
        await stake('pol.fusion', 100000, true)
        await incrementTime(86400)
        await contracts.dapp_contract.actions.stakeallcpu([]).send('mike@active');  
        await incrementTime(86400)
        await contracts.dapp_contract.actions.tgglstakeall(['dapp.fusion']).send('dapp.fusion@active');        
        await contracts.dapp_contract.actions.stakeallcpu([]).send()
        const action = contracts.token_contract.actions.transfer(['pol.fusion', 'dapp.fusion', lswax(100000), 'instant redeem']).send('pol.fusion@active');
        await expectToThrow(action, `eosio_assert: not enough instaredeem funds available`)
    });  

    it('success', async () => {
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(100000), '']).send('mike@active');
        await stake('pol.fusion', 100000, true)
        await contracts.token_contract.actions.transfer(['pol.fusion', 'dapp.fusion', lswax(100000), 'instant redeem']).send('pol.fusion@active');
    });        
});


describe('\n\nrebalance memo', () => {
    
    it('error: only lswax accepted', async () => {
        const action = contracts.wax_contract.actions.transfer(['mike', 'dapp.fusion', wax(10), 'rebalance']).send('mike@active');
        await expectToThrow(action, `eosio_assert: only LSWAX should be sent with this memo`)
    });  

    it('error: sender should be pol.fusion', async () => {
        await stake('mike', 10, true)
        const action = contracts.token_contract.actions.transfer(['mike', 'dapp.fusion', lswax(10), 'rebalance']).send('mike@active');
        await expectToThrow(action, `eosio_assert: expected pol.fusion to be the sender`)
    }); 

    it('error: minimum not met', async () => {
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(10), '']).send('mike@active');
        await stake('pol.fusion', 10, true)
        const action = contracts.token_contract.actions.transfer(['pol.fusion', 'dapp.fusion', lswax(0.9), 'rebalance']).send('pol.fusion@active');
        await expectToThrow(action, `eosio_assert: minimum unliquify amount not met`)
    });      
  
    it('error: not enough instant redeem funds available', async () => {
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(100000), '']).send('mike@active');
        await stake('pol.fusion', 100000, true)
        await incrementTime(86400)
        await contracts.dapp_contract.actions.stakeallcpu([]).send('mike@active');  
        await incrementTime(86400)
        await contracts.dapp_contract.actions.tgglstakeall(['dapp.fusion']).send('dapp.fusion@active');        
        await contracts.dapp_contract.actions.stakeallcpu([]).send()
        const action = contracts.token_contract.actions.transfer(['pol.fusion', 'dapp.fusion', lswax(100000), 'rebalance']).send('pol.fusion@active');
        await expectToThrow(action, `eosio_assert: not enough instaredeem funds available`)
    });  

    it('success', async () => {
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(100000), '']).send('mike@active');
        await stake('pol.fusion', 100000, true)
        await contracts.token_contract.actions.transfer(['pol.fusion', 'dapp.fusion', lswax(100000), 'rebalance']).send('pol.fusion@active');
    });        
});

describe('\n\nwax_lswax_liquidity memo', () => {
    
    it('error: only wax accepted', async () => {
        await stake('mike', 10, true)
        const action = contracts.token_contract.actions.transfer(['mike', 'dapp.fusion', lswax(10), 'wax_lswax_liquidity']).send('mike@active');
        await expectToThrow(action, `eosio_assert: only WAX should be sent with this memo`)
    });  

    it('error: sender should be pol.fusion', async () => {
        const action = contracts.wax_contract.actions.transfer(['mike', 'dapp.fusion', wax(10), 'wax_lswax_liquidity']).send('mike@active');
        await expectToThrow(action, `eosio_assert: expected pol.fusion to be the sender`)
    }); 
      
});

describe('\n\nnew_incentive memo for non-ecosystem farms', () => {

    it('error: alcor pool id does not exist', async () => {
        await incrementTime(86400)
        await contracts.dapp_contract.actions.stakeallcpu([]).send('mike@active');         
        await stake('mike', 1000, true)
        const action = contracts.token_contract.actions.transfer(['mike', 'dapp.fusion', lswax(100), '|new_incentive|100|365|']).send('mike@active');
        await expectToThrow(action, `eosio_assert: alcor pool id does not exist`)
    }); 

    it('error: only lswax accepted', async () => {
        await incrementTime(86400)
        await contracts.dapp_contract.actions.stakeallcpu([]).send('mike@active');         
        const action = contracts.wax_contract.actions.transfer(['mike', 'dapp.fusion', wax(100), '|new_incentive|4|365|']).send('mike@active');
        await expectToThrow(action, `eosio_assert: only LSWAX can be sent with this memo`)        
    });  

    it('error: incomplete memo', async () => {
        await incrementTime(86400)
        await contracts.dapp_contract.actions.stakeallcpu([]).send('mike@active');         
        await stake('mike', 1000, true)
        const action = contracts.token_contract.actions.transfer(['mike', 'dapp.fusion', lswax(100), '|new_incentive|4|']).send('mike@active');
        await expectToThrow(action, `eosio_assert: memo for new_incentive operation is incomplete`) 
    });   

    it('error: duration too short', async () => {
        await incrementTime(86400)
        await contracts.dapp_contract.actions.stakeallcpu([]).send('mike@active');         
        await stake('mike', 1000, true)
        const action = contracts.token_contract.actions.transfer(['mike', 'dapp.fusion', lswax(100), '|new_incentive|4|6|']).send('mike@active');
        await expectToThrow(action, `eosio_assert: duration must be between 7 and 365 days`)
    });  

    it('error: duration too long', async () => {
        await incrementTime(86400)
        await contracts.dapp_contract.actions.stakeallcpu([]).send('mike@active');         
        await stake('mike', 1000, true)
        const action = contracts.token_contract.actions.transfer(['mike', 'dapp.fusion', lswax(100), '|new_incentive|4|366|']).send('mike@active');
        await expectToThrow(action, `eosio_assert: duration must be between 7 and 365 days`)
    });   

    it('error: minimum incentive not met', async () => {
        await incrementTime(86400)
        await contracts.dapp_contract.actions.stakeallcpu([]).send('mike@active');         
        await stake('mike', 1000, true)
        const action = contracts.token_contract.actions.transfer(['mike', 'dapp.fusion', lswax(99.9), '|new_incentive|4|365|']).send('mike@active');
        await expectToThrow(action, `eosio_assert: minimum incentive is 100.00000000 LSWAX`)
    });            
      
    it('error: one of the tokens must be LSWAX', async () => {
        await incrementTime(86400)
        await contracts.dapp_contract.actions.stakeallcpu([]).send('mike@active');         
        await stake('mike', 1000, true)
        const action = contracts.token_contract.actions.transfer(['mike', 'dapp.fusion', lswax(100), '|new_incentive|5|365|']).send('mike@active');
        await expectToThrow(action, `eosio_assert: one of the tokens in the liquidity pool must be LSWAX`)
    });      

    it('success', async () => {
        const alcors_lswax_before = await getBalances('swap.alcor', contracts.token_contract)
        await incrementTime(86400)
        await contracts.dapp_contract.actions.stakeallcpu([]).send('mike@active');     
        const g2 = await getDappGlobal2()    
        await stake('mike', 1000, true)
        await contracts.token_contract.actions.transfer(['mike', 'dapp.fusion', lswax(100), '|new_incentive|3|365|']).send('mike@active');
        const alcors_lswax_after = await getBalances('swap.alcor', contracts.token_contract)
        const expected_added_to_alcor = 100 - parseFloat(g2?.new_incentive_fee)
        assert(parseFloat(alcors_lswax_before[0]?.balance) + expected_added_to_alcor == parseFloat(alcors_lswax_after[0]?.balance), `alcor should have ${expected_added_to_alcor} more lswax` )
    });     
});

describe('\n\nnew_incentive memo for ecosystem farm', () => {
    
    it('error: incentive_id not known yet', async () => {
        await incrementTime(86400)
        await contracts.dapp_contract.actions.stakeallcpu([]).send('mike@active');         
        await stake('mike', 1000, true)
        const action = contracts.token_contract.actions.transfer(['mike', 'dapp.fusion', lswax(100), '|new_incentive|2|365|']).send('mike@active');        
        await expectToThrow(action, `eosio_assert: incentive_id for this pair is not known yet, try again soon`)
    });
       
    it('success: stored as pending boost', async () => {        
        await simulate_days(7, true)
        await stake('mike', 1000, true)
        await incrementTime(86400)
        await contracts.dapp_contract.actions.stakeallcpu([]).send('mike@active');         
        await contracts.dapp_contract.actions.createfarms([]).send('mike@active');
        const alcors_lswax_before = await getBalances('swap.alcor', contracts.token_contract)   
        await contracts.token_contract.actions.transfer(['mike', 'dapp.fusion', lswax(100), '|new_incentive|2|365|']).send('mike@active');
        const incentive_ids = await getIncentiveIdsTable()
        const alcors_lswax_after = await getBalances('swap.alcor', contracts.token_contract)
        assert(incentive_ids[0]?.pending_boosts == lswax(100), "pending boosts should be 100 lswax")
        assert(alcors_lswax_before[0]?.balance == alcors_lswax_after[0]?.balance, "alcors lswax should not have changed")
    }); 

    it('success: pending boost used the next week', async () => {        
        await simulate_days(7, true)
        await stake('mike', 1000, true)
        await incrementTime(86400)
        await contracts.dapp_contract.actions.stakeallcpu([]).send('mike@active');         
        await contracts.dapp_contract.actions.createfarms([]).send('mike@active');
        const alcors_lswax_before = await getBalances('swap.alcor', contracts.token_contract)   
        await contracts.token_contract.actions.transfer(['mike', 'dapp.fusion', lswax(100), '|new_incentive|2|365|']).send('mike@active');
        const incentive_ids = await getIncentiveIdsTable()
        const global_before_extension = await getDappGlobal();
        const alcors_lswax_after = await getBalances('swap.alcor', contracts.token_contract)
        assert(incentive_ids[0]?.pending_boosts == lswax(100), "pending boosts should be 100 lswax")
        assert(alcors_lswax_before[0]?.balance == alcors_lswax_after[0]?.balance, "alcors lswax should not have changed")
        await incrementTime(86400*7)
        await contracts.dapp_contract.actions.createfarms([]).send('mike@active');
        const incentive_ids_after = await getIncentiveIdsTable()
        assert(incentive_ids_after[0]?.pending_boosts == lswax(0), "pending boosts should be 0 lswax")
        const alcors_lswax_3 = await getBalances('swap.alcor', contracts.token_contract)
        const alcor_incentives = await getAlcorIncentives()

        const alcors_expected_balance =     Number(
                                                parseFloat(alcors_lswax_after[0]?.balance)
                                                + 100 // pending_boosts before farm extension
                                                + (parseFloat(global_before_extension?.incentives_bucket) * 0.25)
                                            ).toFixed(8)

        const expected_incentive_amount =   Number(
                                                100 + (parseFloat(global_before_extension?.incentives_bucket) * 0.25)
                                            ).toFixed(8)

        assert(alcors_expected_balance == parseFloat(alcors_lswax_3[0]?.balance), `alcor should have ${alcors_expected_balance} but has ${parseFloat(alcors_lswax_3[0]?.balance)}`)
        assert(expected_incentive_amount == parseFloat(alcor_incentives[0]?.reward?.quantity), `alcor should have ${expected_incentive_amount} but has ${parseFloat(alcor_incentives[0]?.reward?.quantity)}`)
    });           
});