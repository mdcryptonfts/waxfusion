const { blockchain, contracts, incrementTime, init, initial_state, setTime, stake, simulate_days, unliquify } = require("./setup.spec.ts")
const { getPayouts } = require('./notifications.spec.ts')
const { calculate_wax_and_lswax_outputs, lswax, rent_cpu_memo, swax, validate_supply_and_payouts, wax } = require("./helpers.ts")
const { nameToBigInt, TimePoint, expectToThrow } = require("@eosnetwork/vert");
const { Asset, Int64, Name, UInt64, UInt128, TimePointSec } = require('@wharfkit/antelope');
const { assert } = require("chai");

const [mike, bob, ricky, oig] = blockchain.createAccounts('mike', 'bob', 'ricky', 'oig')


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
        const config = await getDappConfig()
        assert( config.admin_wallets.indexOf('mike') > -1, "expected mike to be an admin" );
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
        const config = await getDappConfig()
        assert( config.cpu_contracts.indexOf('mike') > -1, "expected mike to be a cpu contract" );
    });            
});

describe('\n\nclaimaslswax action', () => {

    it('error: missing auth of mike', async () => {
        const action = contracts.dapp_contract.actions.claimaslswax(['mike', lswax(1), 0]).send('eosio@active');
        await expectToThrow(action, "missing required authority mike")
    });  

    it('error: need to use the stake action first', async () => {
        const action = contracts.dapp_contract.actions.claimaslswax(['mike', lswax(1), 0]).send('mike@active');
        await expectToThrow(action, "eosio_assert: you don't have anything staked here")
    });  

    it('error: no rewards to claim', async () => {
        await stake('mike', 100)
        const action = contracts.dapp_contract.actions.claimaslswax(['mike', lswax(1), 0]).send('mike@active');
        await expectToThrow(action, "eosio_assert: you have no wax to claim")
    }); 

    it('error: output symbol mismatch', async () => {
        await stake('mike', 100)
        const action = contracts.dapp_contract.actions.claimaslswax(['mike', wax(1), 0]).send('mike@active');
        await expectToThrow(action, "eosio_assert: output symbol should be LSWAX")
    }); 

    it('error: output amount must be positive', async () => {
        await stake('mike', 100)
        const action = contracts.dapp_contract.actions.claimaslswax(['mike', lswax(0), 0]).send('mike@active');
        await expectToThrow(action, "eosio_assert: Invalid output quantity.")
    });             

    it('error: max slippage out of range', async () => {
        await stake('mike', 100)
        const action = contracts.dapp_contract.actions.claimaslswax(['mike', lswax(1), 100000001]).send('mike@active');
        await expectToThrow(action, "eosio_assert: max slippage is out of range")
    });    

    it('success', async () => {
        await stake('bob', 1000)
        await simulate_days(10)
        await contracts.dapp_contract.actions.claimaslswax(['bob', lswax(0.15), 0]).send('bob@active');
        await getPayouts();
    });                           
});