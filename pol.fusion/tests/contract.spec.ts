const { blockchain, contracts, init, initial_alcor_price, setTime, incrementTime, initial_state, stake } = require("./setup.spec.ts");
const { nameToBigInt, TimePoint, expectToThrow } = require("@eosnetwork/vert");
const { Asset, Int64, Name, UInt64, UInt128, TimePointSec } = require('@wharfkit/antelope');
const { assert } = require("chai");
const { almost_equal, calculate_lswax_to_match_wax, calculate_wax_and_lswax_outputs, calculate_wax_to_match_lswax,
        extend_rental_memo, increase_rental_memo, rent_cpu_memo, lswax, swax, wax } = require('./helpers.ts');

const [mike, bob] = blockchain.createAccounts('mike', 'bob')

//Error messages
const ERR_ALREADY_FUNDED_RENTAL = "eosio_assert: memo for increasing/extending should start with extend_rental or increase_rental"
const ERR_MAX_AMOUNT_TO_RENT = "eosio_assert: maximum wax amount to rent is 10000000.00000000 WAX"
const ERR_MAX_DAYS_TO_RENT = "eosio_assert: maximum days to rent is 3650"
const ERR_MIN_AMOUNT_TO_RENT = "eosio_assert: minimum wax amount to rent is 10.00000000 WAX"
const ERR_MIN_DAYS_TO_EXTEND = "eosio_assert: extension must be at least 1 day"
const ERR_MIN_DAYS_TO_RENT = "eosio_assert: minimum days to rent is 1"
const ERR_NOT_ENOUGH_RENTAL_FUNDS = 'eosio_assert: there is not enough wax in the rental pool to cover this rental'
const ERR_RENTAL_DOESNT_EXIST = "eosio_assert: could not locate an existing rental for this renter/receiver combo"
const ERR_RENTAL_IS_EXPIRED = "eosio_assert: you can't extend a rental after it expired"
const ERR_RENTAL_ISNT_FUNDED = "eosio_assert: you can't extend a rental if it hasnt been funded yet"
const ERR_RENTCPU_FIRST = 'eosio_assert: you need to use the rentcpu action first'

/* Runs before each test */
beforeEach(async () => {
    blockchain.resetTables()
    await init()
})

const scopes = {
    alcor: contracts.alcor_contract.value,
    dapp: contracts.dapp_contract.value,
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

const getSWaxStaker = async (user) => {
    const staker = await contracts.dapp_contract.tables
        .stakers(scopes.dapp)
        .getTableRows(Name.from(user).value.value)[0]
    return staker; 
}


/* Tests */
describe('\n\nverify initial POL state and config', () => {
    //pass `true` to log the results in the console
    it('', async () => {
        await getPolConfig(true)
        await getPolState(true)     
        await getDappTop21(true) 
    });  
});

describe('\n\ntransfer wax and lswax', () => {

    it('wax with no memo', async () => {
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(100000), '']).send('eosio@active');
        const pol_state = await getPolState()
        assert.strictEqual(pol_state.wax_bucket, wax(100000), 'POL should have 100k WAX in bucket');
    });  

    it('lswax with no memo', async () => {
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(12000), 'for staking']).send('eosio@active');
        await stake('mike', 12000, true, 6333)
        await contracts.token_contract.actions.transfer(['mike', 'pol.fusion', lswax(6333), '']).send('mike@active');
        const pol_state = await getPolState()
        assert.strictEqual(pol_state.lswax_bucket, lswax(6333), 'POL should have 6333 LSWAX in bucket');        
    }); 

    it('wax with negative quantity', async () => {
        const action = contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(-100000), '']).send('eosio@active');
        await expectToThrow(action, "eosio_assert: must transfer positive quantity");
    });     

    it('lswax with negative quantity', async () => {
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(12000), 'for staking']).send('eosio@active');
        await stake('mike', 12000, true, 6333)
        const action = contracts.token_contract.actions.transfer(['mike', 'pol.fusion', lswax(-6333), '']).send('mike@active');
        await expectToThrow(action, "eosio_assert: must transfer positive quantity");
    });          

    it('wax with 0 quantity', async () => {
        const action = contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(0), '']).send('eosio@active');
        await expectToThrow(action, "eosio_assert: must transfer positive quantity");
    });     

    it('lswax with 0 quantity', async () => {
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(12000), 'for staking']).send('eosio@active');
        await stake('mike', 12000, true, 6333)
        const action = contracts.token_contract.actions.transfer(['mike', 'pol.fusion', lswax(0), '']).send('mike@active');
        await expectToThrow(action, "eosio_assert: must transfer positive quantity");
    });  

    it('wax with memo that isnt caught by notification handler', async () => {
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(100000), 'some random memo']).send('eosio@active');
        const pol_state = await getPolState()
        assert.strictEqual(pol_state.wax_bucket, wax(100000), 'POL should have 100k WAX in bucket');
    });     

    it('lswax with memo that isnt caught by notification handler', async () => {
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(12000), 'for staking']).send('eosio@active');
        await stake('mike', 12000, true, 6333)
        await contracts.token_contract.actions.transfer(['mike', 'pol.fusion', lswax(420), 'hello']).send('mike@active');
        const pol_state = await getPolState()
        assert.strictEqual(pol_state.lswax_bucket, lswax(420), 'POL should have 420 LSWAX in bucket');        
    });      
});


describe('\n\ndeposit for liquidity only', () => {

    it('when there is 0 lswax and 0 swax circulating', async () => {

        //send wax to POL from eosio
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', '100000.00000000 WAX', 'for liquidity only']).send('eosio@active');
        
        //verify that the state is accurate
        const pol_state = await getPolState()
        assert.strictEqual(pol_state.wax_bucket, wax(0), 'POL should have 0 WAX in bucket');
        assert.strictEqual(pol_state.lswax_bucket, lswax(0), 'POL should have 0 LSWAX in bucket');

        //make sure alcor pools are within 99.99% of the expected amount
        const outputs = calculate_wax_and_lswax_outputs(100000.0, 100000.0 / 95300.0, 1.0);
        const alcor_pool = await getAlcorPool()
        almost_equal(parseFloat(alcor_pool.tokenA.quantity), 100000.0 + outputs[0])
        almost_equal(parseFloat(alcor_pool.tokenB.quantity), 95300.0 + outputs[1])
    });     

    it('when there is 0 lswax and 10,000 swax circulating', async () => {

        //send wax to mike
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(10000), 'for staking']).send('eosio@active');

        //have mike stake the wax, and make sure his staked balance is 10k
        await stake('mike', 10000)
        const mikes_stake = await getSWaxStaker('mike');
        assert.strictEqual(mikes_stake.swax_balance, swax(10000), 'mike should have 10,000 SWAX staked');

        //check the dapp state to make sure there is 10k swax_currently_earning
        const dapp_state = await getDappState()
        assert.strictEqual(dapp_state.swax_currently_earning, swax(10000), 'dapp should have 10,000 swax_currently_earning');

        //transfer 147,315.89 WAX from eosio to POL
        const amount_to_transfer = wax(147315.89)
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', amount_to_transfer, 'for liquidity only']).send('eosio@active');
        
        //validate the POL state
        const pol_state = await getPolState()
        assert.strictEqual(pol_state.wax_bucket, wax(0), 'pol should have 0 WAX');
        assert.strictEqual(pol_state.lswax_bucket, lswax(0), 'pol should have 0 LSWAX');

        //validate that alcors pools are within 99.99% of the expected amounts
        const outputs = calculate_wax_and_lswax_outputs(parseFloat(amount_to_transfer.split(' ')[0]), 100000.0 / 95300.0, 1.0);
        const alcor_pool = await getAlcorPool()
        almost_equal(parseFloat(alcor_pool.tokenA.quantity), 100000.0 + outputs[0])
        almost_equal(parseFloat(alcor_pool.tokenB.quantity), 95300.0 + outputs[1])   

    });    

    it('when there is 5,000 lswax and 6,333 swax_currently_backing_lswax', async () => {

        //send wax to mike
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', '12000.00000000 WAX', 'for staking']).send('eosio@active');

        //have mike stake the wax, and make sure his staked balance is 10k
        await stake('mike', 12000)
        let mikes_stake = await getSWaxStaker('mike');
        assert.strictEqual(mikes_stake.swax_balance, swax(12000), 'mike should have 12,000 SWAX staked');

        //liquify 6,333 swax
        await contracts.dapp_contract.actions.liquify(['mike', swax(6333)]).send('mike@active');

        //check lswax supply, mikes stake, and mikes lswax balance
        const lswax_supply = await getSupply(contracts.token_contract, 'LSWAX');
        assert.strictEqual(lswax_supply.supply, lswax(6333), 'LSWAX supply should be 6333');

        mikes_stake = await getSWaxStaker('mike');
        assert.strictEqual(mikes_stake.swax_balance, swax(5667), 'mike should have 5667.00000000 SWAX staked');        

        const mikes_lswax_balance = await getBalances('mike', contracts.token_contract);
        assert.strictEqual(mikes_lswax_balance[0].balance, lswax(6333), 'mike should have 6333.00000000 LSWAX');   

        //transfer wax to pol for liquidity
        const amount_to_transfer = wax(147315.89)
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', amount_to_transfer, 'for liquidity only']).send('eosio@active');             
    
        //validate the POL state
        const pol_state = await getPolState()
        assert.strictEqual(pol_state.wax_bucket, wax(0), 'pol should have 0 WAX');
        assert.strictEqual(pol_state.lswax_bucket, lswax(0), 'pol should have 0 LSWAX');

        //validate that alcors pools are within 99.99% of the expected amounts
        let outputs = calculate_wax_and_lswax_outputs(parseFloat(amount_to_transfer.split(' ')[0]), 100000.0 / 95300.0, 1.0 );
        const alcor_pool = await getAlcorPool()
        almost_equal(parseFloat(alcor_pool.tokenA.quantity), 100000.0 + outputs[0])
        almost_equal(parseFloat(alcor_pool.tokenB.quantity), 95300.0 + outputs[1])  

    });     
          
    it('when alcor pair has 0 assets in it', async () => {

        //send wax to mike
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', '12000.00000000 WAX', 'for staking']).send('eosio@active');

        //have mike stake the wax, and make sure his staked balance is 10k
        await stake('mike', 12000)
        let mikes_stake = await getSWaxStaker('mike');
        assert.strictEqual(mikes_stake.swax_balance, swax(12000), 'mike should have 12,000 SWAX staked');

        //liquify 6,333 swax
        await contracts.dapp_contract.actions.liquify(['mike', swax(6333)]).send('mike@active');

        //check lswax supply, mikes stake, and mikes lswax balance
        const lswax_supply = await getSupply(contracts.token_contract, 'LSWAX');
        assert.strictEqual(lswax_supply.supply, lswax(6333), 'LSWAX supply should be 6333');

        mikes_stake = await getSWaxStaker('mike');
        assert.strictEqual(mikes_stake.swax_balance, swax(5667), 'mike should have 5667.00000000 SWAX staked');        

        const mikes_lswax_balance = await getBalances('mike', contracts.token_contract);
        assert.strictEqual(mikes_lswax_balance[0].balance, lswax(6333), 'mike should have 6333.00000000 LSWAX');   

        //re-initialize alcor pools with 0 assets, then validate the alcor state
        await contracts.alcor_contract.actions.initunittest([wax(0), lswax(0)]).send();    
        let alcor_pool = await getAlcorPool()
        assert.strictEqual(alcor_pool.tokenA.quantity, wax(0), 'alcor should have 0 WAX');
        assert.strictEqual(alcor_pool.tokenB.quantity, lswax(0), 'alcor should have 0 LSWAX');

        //transfer wax to pol for liquidity
        const amount_to_transfer = wax(147315.89)
        const expected_quantity = parseFloat(147315.89 / 2).toFixed(8)
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', amount_to_transfer, 'for liquidity only']).send('eosio@active');             
    
        //validate the POL state
        const pol_state = await getPolState()
        assert.strictEqual(pol_state.wax_bucket, wax(0), 'pol should have 0 WAX');
        assert.strictEqual(pol_state.lswax_bucket, lswax(0), 'pol should have 0 LSWAX');

        //validate that alcors pools have exactly half of the wax in each side of the pool
        alcor_pool = await getAlcorPool()
        assert.strictEqual(alcor_pool.tokenA.quantity, wax(expected_quantity), `alcor should have ${wax(expected_quantity)}`);
        assert.strictEqual(alcor_pool.tokenB.quantity, lswax(expected_quantity), `alcor should have ${lswax(expected_quantity)}`);
        
    });  

    it('when alcor pair has 0 of one asset but positive quantity of the other', async () => {

        //send wax to mike
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', '12000.00000000 WAX', 'for staking']).send('eosio@active');

        //have mike stake the wax, and make sure his staked balance is 10k
        await stake('mike', 12000)
        let mikes_stake = await getSWaxStaker('mike');
        assert.strictEqual(mikes_stake.swax_balance, swax(12000), 'mike should have 12,000 SWAX staked');

        //liquify 6,333 swax
        await contracts.dapp_contract.actions.liquify(['mike', swax(6333)]).send('mike@active');

        //check lswax supply, mikes stake, and mikes lswax balance
        const lswax_supply = await getSupply(contracts.token_contract, 'LSWAX');
        assert.strictEqual(lswax_supply.supply, lswax(6333), 'LSWAX supply should be 6333');

        mikes_stake = await getSWaxStaker('mike');
        assert.strictEqual(mikes_stake.swax_balance, swax(5667), 'mike should have 5667.00000000 SWAX staked');        

        const mikes_lswax_balance = await getBalances('mike', contracts.token_contract);
        assert.strictEqual(mikes_lswax_balance[0].balance, lswax(6333), 'mike should have 6333.00000000 LSWAX');   

        //re-initialize alcor pools with 0 assets, then validate the alcor state
        await contracts.alcor_contract.actions.initunittest([wax(0), lswax(50)]).send();    
        let alcor_pool = await getAlcorPool()
        assert.strictEqual(alcor_pool.tokenA.quantity, wax(0), 'alcor should have 0 WAX');
        assert.strictEqual(alcor_pool.tokenB.quantity, lswax(50), 'alcor should have 50 LSWAX');

        //transfer wax to pol for liquidity
        const amount_to_transfer = wax(147315.89)
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', amount_to_transfer, 'for liquidity only']).send('eosio@active');             
    
        //pol should have not bought any LSWAX, and added all WAX into the wax_bucket
        const pol_state = await getPolState()
        assert.strictEqual(pol_state.wax_bucket, amount_to_transfer, `pol should have ${amount_to_transfer}`);
        assert.strictEqual(pol_state.lswax_bucket, lswax(0), 'pol should have 0 LSWAX');

        //validate that alcors pools did not receive a new deposit
        alcor_pool = await getAlcorPool()
        assert.strictEqual(alcor_pool.tokenA.quantity, wax(0), 'alcor should have 0 WAX');
        assert.strictEqual(alcor_pool.tokenB.quantity, lswax(50), 'alcor should have 50 LSWAX');

    });    

    it('when alcor price is out of range, but has positive quantities of both assets', async () => {

        //send wax to mike
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', '12000.00000000 WAX', 'for staking']).send('eosio@active');

        //have mike stake the wax, and make sure his staked balance is 10k
        await stake('mike', 12000)
        let mikes_stake = await getSWaxStaker('mike');
        assert.strictEqual(mikes_stake.swax_balance, swax(12000), 'mike should have 12,000 SWAX staked');

        //liquify 6,333 swax
        await contracts.dapp_contract.actions.liquify(['mike', swax(6333)]).send('mike@active');

        //check lswax supply, mikes stake, and mikes lswax balance
        const lswax_supply = await getSupply(contracts.token_contract, 'LSWAX');
        assert.strictEqual(lswax_supply.supply, lswax(6333), 'LSWAX supply should be 6333');

        mikes_stake = await getSWaxStaker('mike');
        assert.strictEqual(mikes_stake.swax_balance, swax(5667), 'mike should have 5667.00000000 SWAX staked');        

        const mikes_lswax_balance = await getBalances('mike', contracts.token_contract);
        assert.strictEqual(mikes_lswax_balance[0].balance, lswax(6333), 'mike should have 6333.00000000 LSWAX');   

        //re-initialize alcor pools with 0 assets, then validate the alcor state
        await contracts.alcor_contract.actions.initunittest([wax(10000), lswax(9000)]).send();    
        let alcor_pool = await getAlcorPool()
        assert.strictEqual(alcor_pool.tokenA.quantity, wax(10000), 'alcor should have 10,000 WAX');
        assert.strictEqual(alcor_pool.tokenB.quantity, lswax(9000), 'alcor should have 9,000 LSWAX');

        //transfer wax to pol for liquidity
        const amount_to_transfer = wax(147315.89)
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', amount_to_transfer, 'for liquidity only']).send('eosio@active');             
    
        //pol should have not bought any LSWAX, and added all WAX into the wax_bucket
        const pol_state = await getPolState()
        assert.strictEqual(pol_state.wax_bucket, amount_to_transfer, `pol should have ${amount_to_transfer}`);
        assert.strictEqual(pol_state.lswax_bucket, lswax(0), 'pol should have 0 LSWAX');

        //validate that alcors pools did not receive a new deposit
        alcor_pool = await getAlcorPool()
        assert.strictEqual(alcor_pool.tokenA.quantity, wax(10000), 'alcor should have 10,000 WAX');
        assert.strictEqual(alcor_pool.tokenB.quantity, lswax(9000), 'alcor should have 9,000 LSWAX');

    });    

    it('when alcor price is more than 5% below the real price (peg was lost)', async () => {

        //send wax to mike
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(12000), 'for staking']).send('eosio@active');

        //have mike stake the wax, and make sure his staked balance is 10k
        await stake('mike', 12000)
        let mikes_stake = await getSWaxStaker('mike');
        assert.strictEqual(mikes_stake.swax_balance, swax(12000), 'mike should have 12,000 SWAX staked');

        //liquify 6,333 swax
        await contracts.dapp_contract.actions.liquify(['mike', swax(6333)]).send('mike@active');

        //check lswax supply, mikes stake, and mikes lswax balance
        const lswax_supply = await getSupply(contracts.token_contract, 'LSWAX');
        assert.strictEqual(lswax_supply.supply, lswax(6333), 'LSWAX supply should be 6333');

        mikes_stake = await getSWaxStaker('mike');
        assert.strictEqual(mikes_stake.swax_balance, swax(5667), 'mike should have 5667.00000000 SWAX staked');        

        const mikes_lswax_balance = await getBalances('mike', contracts.token_contract);
        assert.strictEqual(mikes_lswax_balance[0].balance, lswax(6333), 'mike should have 6333.00000000 LSWAX');   

        //re-initialize alcor pools with 0 assets, then validate the alcor state
        await contracts.alcor_contract.actions.initunittest([wax(50000), lswax(90000)]).send();    
        let alcor_pool = await getAlcorPool()
        assert.strictEqual(alcor_pool.tokenA.quantity, wax(50000), 'alcor should have 50,000 WAX');
        assert.strictEqual(alcor_pool.tokenB.quantity, lswax(90000), 'alcor should have 90,000 LSWAX');

        //transfer wax to pol for liquidity
        const amount_to_transfer = wax(147315.89)
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', amount_to_transfer, 'for liquidity only']).send('eosio@active');             
    
        //pol should not have done anything since the price is too low on alcor
        const pol_state = await getPolState()
        assert.strictEqual(pol_state.wax_bucket, amount_to_transfer, `pol should have ${amount_to_transfer}`);
        assert.strictEqual(pol_state.lswax_bucket, lswax(0), 'pol should have 0 LSWAX');

        //validate that alcors pools are within 99.99% of the expected amounts
        alcor_pool = await getAlcorPool()
        assert.strictEqual(alcor_pool.tokenA.quantity, wax(50000), 'alcor should have 50,000 WAX');
        assert.strictEqual(alcor_pool.tokenB.quantity, lswax(90000), 'alcor should have 90,000 LSWAX');

    });                 
});

describe('\n\ndeposit for CPU rental pool only', () => {

    it('send 100k wax', async () => {

        //send wax to POL from eosio
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(100000), 'for staking pool only']).send('eosio@active');
        
        //state should have empty buckets, and 100k wax in wax_available_for_rentals
        const pol_state = await getPolState()
        assert.strictEqual(pol_state.wax_bucket, wax(0), 'POL should have 0 WAX in bucket');
        assert.strictEqual(pol_state.lswax_bucket, lswax(0), 'POL should have 0 LSWAX in bucket');
        assert.strictEqual(pol_state.wax_available_for_rentals, wax(100000), 'POL should have 100k WAX for rentals');

    }); 

});

describe('\n\nrebalance memo, sending 100k wax', () => {
    //its important to note that these tests are just a safety measure
    //there is no scenario where the pol contract should receive a 'rebalance' memo
    //from the dapp contract, unless the proper amounts were already calculated
    //by the POL contract, using the rebalance action

    it('when both buckets start off empty', async () => {

        //send wax to dapp.fusion first since rebalance memo should only come from dapp.fusion
        await contracts.wax_contract.actions.transfer(['eosio', 'dapp.fusion', wax(100000), '']).send('eosio@active');

        //validate dapp.fusion wax balance
        const dapp_wax_balance = await getBalances('dapp.fusion', contracts.wax_contract);
        assert.strictEqual(dapp_wax_balance[0].balance, wax(100000), 'dapp should have 100000.00000000 WAX');

        //transfer from dapp to POL with rebalance memo
        await contracts.wax_contract.actions.transfer(['dapp.fusion', 'pol.fusion', wax(100000), 'rebalance']).send('dapp.fusion@active');
        
        //validate POL balance
        const pol_wax_balance = await getBalances('pol.fusion', contracts.wax_contract);
        assert.strictEqual(pol_wax_balance[0].balance, wax(100000), 'POL should have 100000.00000000 WAX');        

        //since we have no LSWAX, this wax all should've gone into the wax bucket
        const pol_state = await getPolState();
        assert.strictEqual(pol_state.wax_bucket, wax(100000), 'POL should have 100k WAX in bucket');
        assert.strictEqual(pol_state.lswax_bucket, lswax(0), 'POL should have 0 LSWAX in bucket');       

    }); 

    it('when the lswax bucket has 20,000 LSWAX and wax bucket is empty', async () => {

        //stake 20,000 WAX and liquify it, then send it to pol with liquidity memo
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(20000), '']).send('eosio@active');
        await stake('mike', 20000, true)
        await contracts.token_contract.actions.transfer(['mike', 'pol.fusion', lswax(20000), 'liquidity']).send('mike@active');

        //validate that the LSWAX bucket has 20k LSWAX in it
        let pol_state = await getPolState();
        assert.strictEqual(pol_state.lswax_bucket, lswax(20000), 'POL should have 20k LSWAX in bucket');      

        //send wax to dapp.fusion first since rebalance memo should only come from dapp.fusion
        await contracts.wax_contract.actions.transfer(['eosio', 'dapp.fusion', wax(100000), '']).send('eosio@active');

        //validate dapp.fusion wax balance
        const dapp_wax_balance = await getBalances('dapp.fusion', contracts.wax_contract);
        assert.strictEqual(dapp_wax_balance[0].balance, wax(120000), 'dapp should have 120000.00000000 WAX');

        //transfer from dapp to POL with rebalance memo
        await contracts.wax_contract.actions.transfer(['dapp.fusion', 'pol.fusion', wax(100000), 'rebalance']).send('dapp.fusion@active'); 

        //since we had 20k LSWAX, a matching weight of wax should've gotten paired with it
        //lswax bucket should now be empty and alcor should have 115300 LSWAX
        //pol's wax bucket should should have ( 100k - (20k * alcors_price) )
        const wax_spent = calculate_wax_to_match_lswax(20000.0, 100000.0 / 95300.0, 1.0 );
        const expected_wax_bucket = 100000.0 - wax_spent;
        pol_state = await getPolState();
        almost_equal(parseFloat(pol_state.wax_bucket.split(' ')[0]), expected_wax_bucket);
        assert.strictEqual(pol_state.lswax_bucket, lswax(0), 'pol should have 0 LSWAX');
    
        //check alcor's balances
        alcor_pool = await getAlcorPool()
        const expected_alcor_wax_bucket = 100000.0 + parseFloat(wax_spent);
        almost_equal(parseFloat(alcor_pool.tokenA.quantity.split(' ')[0]), expected_alcor_wax_bucket);
        assert.strictEqual(alcor_pool.tokenB.quantity, lswax(115300), 'alcor should have 115300 LSWAX');        

    });   

    it('when the wax bucket has 20,000 WAX and LSWAX bucket is empty', async () => {

        //transfer 20k wax to pol contract
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(20000), '']).send('eosio@active');

        //validate that the WAX bucket has 20k WAX in it
        let pol_state = await getPolState();
        assert.strictEqual(pol_state.wax_bucket, wax(20000), 'POL should have 20k WAX in bucket');      

        //send wax to dapp.fusion first since rebalance memo should only come from dapp.fusion
        await contracts.wax_contract.actions.transfer(['eosio', 'dapp.fusion', wax(100000), '']).send('eosio@active');
        await contracts.wax_contract.actions.transfer(['dapp.fusion', 'pol.fusion', wax(100000), 'rebalance']).send('dapp.fusion@active'); 

        //nothing should have happened, so the POL wax bucket should now have 120k wax
        pol_state = await getPolState();
        assert.strictEqual(pol_state.wax_bucket, wax(120000), 'pol should have 120k WAX');
        assert.strictEqual(pol_state.lswax_bucket, lswax(0), 'pol should have 0 LSWAX');
    
        //check alcor's balances, they should still be at initial state
        alcor_pool = await getAlcorPool()
        assert.strictEqual(alcor_pool.tokenA.quantity, wax(100000), 'alcor should have 100k WAX');
        assert.strictEqual(alcor_pool.tokenB.quantity, lswax(95300), 'alcor should have 95300 LSWAX');
    });    

    it('when lswax is the limiting factor but both buckets are positive', async () => {

        //send 1k lswax to pol
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(20000), '']).send('eosio@active');
        await stake('mike', 1000, true)
        await contracts.token_contract.actions.transfer(['mike', 'pol.fusion', lswax(1000), '']).send('mike@active');        

        //transfer 20k wax to pol contract
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(20000), '']).send('eosio@active');

        //validate that the WAX bucket has 20k WAX in it
        let pol_state = await getPolState();
        assert.strictEqual(pol_state.wax_bucket, wax(20000), 'POL should have 20k WAX in bucket');    
        assert.strictEqual(pol_state.lswax_bucket, lswax(1000), 'POL should have 1k LSWAX in bucket');      

        //send wax to dapp.fusion first since rebalance memo should only come from dapp.fusion
        await contracts.wax_contract.actions.transfer(['eosio', 'dapp.fusion', wax(100000), '']).send('eosio@active');
        await contracts.wax_contract.actions.transfer(['dapp.fusion', 'pol.fusion', wax(100000), 'rebalance']).send('dapp.fusion@active'); 

        //pol should have spent all lswax and the max wax possible
        pol_state = await getPolState();
        const wax_spent = 1000 * initial_alcor_price()
        const wax_bucket_after = parseFloat(120000 - wax_spent).toFixed(8);
        const alcor_wax_after = parseFloat(100000 + wax_spent).toFixed(8);
        almost_equal(parseFloat(pol_state.wax_bucket), wax_bucket_after);
        assert.strictEqual(pol_state.lswax_bucket, lswax(0), 'pol should have 0 LSWAX');
    
        //check alcor's balances
        alcor_pool = await getAlcorPool()
        almost_equal(parseFloat(alcor_pool.tokenA.quantity), alcor_wax_after);
        assert.strictEqual(alcor_pool.tokenB.quantity, lswax(96300), 'alcor should have 96300 LSWAX');
    });  


    it('when wax is the limiting factor but both buckets are positive', async () => {

        //send 20k lswax to pol
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(20000), '']).send('eosio@active');
        await stake('mike', 20000, true)
        await contracts.token_contract.actions.transfer(['mike', 'pol.fusion', lswax(20000), '']).send('mike@active');        

        //transfer 1k wax to pol contract
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), '']).send('eosio@active');

        //validate that the WAX bucket has 1k WAX in it
        let pol_state = await getPolState();
        assert.strictEqual(pol_state.wax_bucket, wax(1000), 'POL should have 1k WAX in bucket');    
        assert.strictEqual(pol_state.lswax_bucket, lswax(20000), 'POL should have 20k LSWAX in bucket');      

        //send wax to dapp.fusion first since rebalance memo should only come from dapp.fusion
        await contracts.wax_contract.actions.transfer(['eosio', 'dapp.fusion', wax(1000), '']).send('eosio@active');
        await contracts.wax_contract.actions.transfer(['dapp.fusion', 'pol.fusion', wax(1000), 'rebalance']).send('dapp.fusion@active'); 

        //pol should have spent all lswax and the max wax possible
        pol_state = await getPolState();

        //lswax spent should be the amount that matches 2000 wax based on alcors price
        
        const lswax_spent = 2000 / initial_alcor_price()
        const lswax_bucket_after = parseFloat(20000 - lswax_spent).toFixed(8);
        const alcor_lswax_after = parseFloat(95300 + lswax_spent).toFixed(8);
        almost_equal(parseFloat(pol_state.lswax_bucket), lswax_bucket_after);
        assert.strictEqual(pol_state.wax_bucket, wax(0), 'pol should have 0 WAX');
    
        //check alcor's balances
        alcor_pool = await getAlcorPool()
        almost_equal(parseFloat(alcor_pool.tokenB.quantity), alcor_lswax_after);
        assert.strictEqual(alcor_pool.tokenA.quantity, wax(102000), 'alcor should have 102k WAX');
    });            

    it('alcor is out of range', async () => {
        //send lswax to pol
        await contracts.alcor_contract.actions.initunittest([wax(10000), lswax(5000)]).send();   
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(20000), '']).send('eosio@active');
        await stake('mike', 20000, true)
        await contracts.token_contract.actions.transfer(['mike', 'pol.fusion', lswax(20000), '']).send('mike@active');        

        //transfer 1k wax to pol contract
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), '']).send('eosio@active');

        //validate that the WAX bucket has 1k WAX in it
        let pol_state = await getPolState();
        assert.strictEqual(pol_state.wax_bucket, wax(1000), 'POL should have 1k WAX in bucket');    
        assert.strictEqual(pol_state.lswax_bucket, lswax(20000), 'POL should have 20k LSWAX in bucket');      

        //send wax to dapp.fusion first since rebalance memo should only come from dapp.fusion
        await contracts.wax_contract.actions.transfer(['eosio', 'dapp.fusion', wax(10000), '']).send('eosio@active');

        //transfer from dapp to POL with rebalance memo
        await contracts.wax_contract.actions.transfer(['dapp.fusion', 'pol.fusion', wax(10000), 'rebalance']).send('dapp.fusion@active'); 

        //pol should have spent all lswax and the max wax possible
        pol_state = await getPolState();
        assert.strictEqual(pol_state.wax_bucket, wax(11000), 'POL should have 11k WAX in bucket');    
        assert.strictEqual(pol_state.lswax_bucket, lswax(20000), 'POL should have 20k LSWAX in bucket'); 

        //check alcor's balances
        alcor_pool = await getAlcorPool()
        assert.strictEqual(alcor_pool.tokenA.quantity, wax(10000), 'alcor should have 10k WAX');
        assert.strictEqual(alcor_pool.tokenB.quantity, lswax(5000), 'alcor should have 5k LSWAX');
    }); 

});

describe('\n\nPOL allocation from waxfusion distribution', () => {

    it('when alcor price is in range', async () => {

        //send wax to dapp.fusion first since pol allocation memo should only come from dapp.fusion
        await contracts.wax_contract.actions.transfer(['eosio', 'dapp.fusion', wax(1000), '']).send('eosio@active');
        await contracts.wax_contract.actions.transfer(['dapp.fusion', 'pol.fusion', wax(1000), 'pol allocation from waxfusion distribution']).send('dapp.fusion@active');
        
        //pol cpu bucket should have 1/7 of 1000 wax
        //wax and lswax buckets should be empty
        const expected_wax = `${parseFloat(1000.0 - (1000.0 / 7.0 * 6.0)).toFixed(8)}`
        const pol_state = await getPolState();
        assert.strictEqual(pol_state.wax_bucket, wax(0), `POL should have 0 WAX in bucket`);
        assert.strictEqual(pol_state.lswax_bucket, lswax(0), 'POL should have 0 LSWAX in bucket');   
        almost_equal(parseFloat(pol_state.wax_available_for_rentals.split(' ')[0]), expected_wax); 

        //check alcor pool, should have initial amounts + allocations from 6/7 of 1000 wax
        alcor_pool = await getAlcorPool()
        let outputs = calculate_wax_and_lswax_outputs(1000 - expected_wax, 100000.0 / 95300.0, 1.0 );
        almost_equal(parseFloat(alcor_pool.tokenA.quantity), 100000.0 + outputs[0])
        almost_equal(parseFloat(alcor_pool.tokenB.quantity), 95300.0 + outputs[1]) ;
    }); 

    it('when the rental_pool_allocation is 0%', async () => {

        //setallocs to 100000000 (100% liquidity)
        await contracts.pol_contract.actions.setallocs([100000000]).send('pol.fusion@active');

        //send wax to dapp.fusion first since pol allocation memo should only come from dapp.fusion
        await contracts.wax_contract.actions.transfer(['eosio', 'dapp.fusion', wax(1000), '']).send('eosio@active');
        await contracts.wax_contract.actions.transfer(['dapp.fusion', 'pol.fusion', wax(1000), 'pol allocation from waxfusion distribution']).send('dapp.fusion@active');
        
        //pol cpu bucket should have nothing
        //wax and lswax buckets should be empty
        const pol_state = await getPolState();
        assert.strictEqual(pol_state.wax_bucket, wax(0), `POL should have 0 WAX in bucket`);
        assert.strictEqual(pol_state.lswax_bucket, lswax(0), 'POL should have 0 LSWAX in bucket');   
        assert.strictEqual(pol_state.wax_available_for_rentals, wax(0), 'POL should have 0 WAX for rentals');   

        //check alcor pool, should have initial amounts + allocations from 6/7 of 1000 wax
        alcor_pool = await getAlcorPool()
        let outputs = calculate_wax_and_lswax_outputs(1000, 100000.0 / 95300.0, 1.0 );
        almost_equal(parseFloat(alcor_pool.tokenA.quantity), 100000.0 + outputs[0])
        almost_equal(parseFloat(alcor_pool.tokenB.quantity), 95300.0 + outputs[1]);
    });   

});


describe('\n\ndeposit LSWAX for liquidity', () => {

    it('when there is 0 WAX in the bucket', async () => {
        //send LSWAX to pol.fusion (in DEBUG mode it will accept from any account)
        await stake('eosio', 1000, true)
        await contracts.token_contract.actions.transfer(['eosio', 'pol.fusion', lswax(1000), 'liquidity']).send('eosio@active');

        //wax bucket should be empty, lswax bucket should have 1000 LSWAX
        const pol_state = await getPolState();
        assert.strictEqual(pol_state.wax_bucket, wax(0), `POL should have 0 WAX in bucket`);
        assert.strictEqual(pol_state.lswax_bucket, lswax(1000), 'POL should have 1000 LSWAX in bucket');   
    }); 

    it('when there is positive WAX quantity, but less weight than LSWAX', async () => {

        //transfer 1000 WAX from eosio to pol with no memo
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), '']).send('eosio@active');
        await stake('eosio', 3000, true)

        //send LSWAX to pol.fusion (in DEBUG mode it will accept from any account)
        await contracts.token_contract.actions.transfer(['eosio', 'pol.fusion', lswax(3000), 'liquidity']).send('eosio@active');

        //calculate the amount of lswax spent to pair with 1000 wax
        const lswax_spent = calculate_lswax_to_match_wax(1000.0, 100000.0 / 95300.0, 1.0 );

        //wax bucket should be empty, lswax bucket should have 3000 - lswax_spent LSWAX
        const pol_state = await getPolState();
        assert.strictEqual(pol_state.wax_bucket, wax(0), `POL should have 0 WAX in bucket`);
        almost_equal(3000.0 - parseFloat(pol_state.lswax_bucket), lswax_spent);

        //alcor should have 1000 wax more, and lswax_spent more than initial
        alcor_pool = await getAlcorPool()
        const alcor_amount_expected = 95300.0 + parseFloat(lswax_spent)
        assert.strictEqual(alcor_pool.tokenA.quantity, wax(101000), `alcor should have 101k WAX`);
        almost_equal(parseFloat(alcor_pool.tokenB.quantity), alcor_amount_expected);
    });   

});


describe('\n\nsend rent_cpu memo', () => {

    it('error: use rentcpu action first', async () => {
        const memo = rent_cpu_memo( 'mike', 10, 1000 ); //receiver, days, wax
        const action = contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(10), memo]).send('eosio@active'); 
        await expectToThrow(action, ERR_RENTCPU_FIRST);
    }); 

    it('error: incomplete memo', async () => {
        const memo = '|rent_cpu|mike|3|'
        await contracts.pol_contract.actions.rentcpu(['eosio', 'mike']).send('eosio@active');  
        const action = contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(10), memo]).send('eosio@active');  
        await expectToThrow(action, 'eosio_assert: memo for rent_cpu operation is incomplete');
    });     

    it('error: rental pool has no wax', async () => {
        const memo = rent_cpu_memo( 'mike', 10, 1000 ); //receiver, days, wax
        await contracts.pol_contract.actions.rentcpu(['eosio', 'mike']).send('eosio@active');  
        const action = contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(10), memo]).send('eosio@active');  
        await expectToThrow(action, ERR_NOT_ENOUGH_RENTAL_FUNDS);
    }); 

    it('user sent more wax than necessary', async () => {
        //send to mike first and have mike rent to track the wax balance
        const memo = rent_cpu_memo( 'eosio', 10, 1000 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(20), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(20), memo]).send('mike@active');  
        const mikes_wax_balance = await getBalances('mike', contracts.wax_contract)
        assert.strictEqual(mikes_wax_balance[0].balance, wax(8), `mike should have 8 wax`);
    });     

    it('error: didnt send enough wax', async () => {
        //send to mike first and have mike rent to track the wax balance
        const memo = rent_cpu_memo( 'eosio', 10, 1000 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(10), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        const action = contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(10), memo]).send('mike@active');  
        await expectToThrow(action, 'eosio_assert: expected to receive 12.00000000 WAX');
    });  

    it('error: already funded rental', async () => {
        //send to mike first and have mike rent to track the wax balance
        const memo = rent_cpu_memo( 'eosio', 10, 1000 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(20), memo]).send('mike@active');  
        const action = contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(20), memo]).send('mike@active');  
        await expectToThrow(action, ERR_ALREADY_FUNDED_RENTAL);
    });  

    it('error: minimum days to rent', async () => {
        //send to mike first and have mike rent to track the wax balance
        const memo = rent_cpu_memo( 'eosio', 0, 1000 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        const action = contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(20), memo]).send('mike@active');  
        await expectToThrow(action, ERR_MIN_DAYS_TO_RENT);
    });   

    it('error: maximum days to rent', async () => {
        //send to mike first and have mike rent to track the wax balance
        const memo = rent_cpu_memo( 'eosio', 1000000000, 1000 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        const action = contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(20), memo]).send('mike@active');  
        await expectToThrow(action, ERR_MAX_DAYS_TO_RENT);
    });   

    it('error: negative number passed for days to rent', async () => {
        //send to mike first and have mike rent to track the wax balance
        const memo = rent_cpu_memo( 'eosio', -100, 1000 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        const action = contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(20), memo]).send('mike@active');  
        //negative number will wrap unsigned int
        await expectToThrow(action, ERR_MAX_DAYS_TO_RENT);
    });  

    it('error: negative number passed for wax amount to rent', async () => {
        //send to mike first and have mike rent to track the wax balance
        const memo = rent_cpu_memo( 'eosio', 10, -1000 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        const action = contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(20), memo]).send('mike@active');  
        //negative number will wrap unsigned int
        await expectToThrow(action, ERR_MAX_AMOUNT_TO_RENT);
    });   

    it('error: minimum amount to rent', async () => {
        //send to mike first and have mike rent to track the wax balance
        const memo = rent_cpu_memo( 'eosio', 10, 5 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        const action = contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(20), memo]).send('mike@active');  
        await expectToThrow(action, ERR_MIN_AMOUNT_TO_RENT);
    });  

    it('exact amount was sent', async () => {
        //send to mike first and have mike rent to track the wax balance
        const memo = rent_cpu_memo( 'eosio', 10, 1000 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(12), memo]).send('mike@active');  
        const mikes_wax_balance = await getBalances('mike', contracts.wax_contract)
        assert.strictEqual(mikes_wax_balance[0].balance, wax(188), `mike should have 188 wax`);        
    });                     
});


describe('\n\nsend extend_rental memo', () => {

    it('error: rental doesnt exist', async () => {
        const memo = extend_rental_memo( 'mike', 10 ); //receiver, days
        const action = contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(10), memo]).send('eosio@active'); 
        await expectToThrow(action, ERR_RENTAL_DOESNT_EXIST);
    }); 
            
    it('error: rental is expired', async () => {
        //rent cpu for 30 days
        let memo = rent_cpu_memo( 'eosio', 30, 1000 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(36), memo]).send('mike@active');        
        
        //fast forward time 40 days
        await setTime(initial_state.chain_time + (60 * 60 * 24 * 40));

        //extend
        memo = extend_rental_memo( 'eosio', 10 ); //receiver, days
        const action = contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(10), memo]).send('mike@active'); 
        await expectToThrow(action, ERR_RENTAL_IS_EXPIRED);
    });  

    it('error: rental was not funded yet', async () => {
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');                 

        //extend
        const memo = extend_rental_memo( 'eosio', 10 ); //receiver, days
        const action = contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(10), memo]).send('mike@active'); 
        await expectToThrow(action, ERR_RENTAL_ISNT_FUNDED);
    });   

    it('error: minimum days to extend', async () => {
        //rent cpu for 30 days
        let memo = rent_cpu_memo( 'eosio', 30, 1000 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(36), memo]).send('mike@active');               

        //extend
        memo = extend_rental_memo( 'eosio', 0 ); //receiver, days
        const action = contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(10), memo]).send('mike@active'); 
        await expectToThrow(action, ERR_MIN_DAYS_TO_EXTEND);
    }); 

    it('error: maximum days to extend', async () => {
        //rent cpu for 30 days
        let memo = rent_cpu_memo( 'eosio', 30, 1000 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(36), memo]).send('mike@active');               

        //extend
        memo = extend_rental_memo( 'eosio', 3651 ); //receiver, days
        const action = contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(10), memo]).send('mike@active'); 
        await expectToThrow(action, ERR_MAX_DAYS_TO_RENT);
    });  

    it('error: didnt send enough funds', async () => {
        //rent cpu for 30 days
        let memo = rent_cpu_memo( 'eosio', 30, 1000 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(36), memo]).send('mike@active');               

        //extend
        memo = extend_rental_memo( 'eosio', 1000 ); //receiver, days
        const action = contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(10), memo]).send('mike@active'); 
        await expectToThrow(action, "eosio_assert: expected to receive 1200.00000000 WAX");
    });     

    it('user sent more funds than needed', async () => {
        //rent cpu for 30 days
        let memo = rent_cpu_memo( 'eosio', 30, 1000 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(36), memo]).send('mike@active');               

        //extend
        memo = extend_rental_memo( 'eosio', 30 ); //receiver, days
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(63), memo]).send('mike@active'); 
        const mikes_wax_balance = await getBalances('mike', contracts.wax_contract)
        assert.strictEqual(mikes_wax_balance[0].balance, wax(128), `mike should have 128 wax`); 
    });   

    it('exact amount was sent', async () => {
        //rent cpu for 30 days
        let memo = rent_cpu_memo( 'eosio', 30, 1000 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(36), memo]).send('mike@active');               

        //extend
        memo = extend_rental_memo( 'eosio', 30 ); //receiver, days
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(36), memo]).send('mike@active'); 
        const mikes_wax_balance = await getBalances('mike', contracts.wax_contract)
        assert.strictEqual(mikes_wax_balance[0].balance, wax(128), `mike should have 128 wax`); 
    });    

    it('incomplete memo', async () => {
        //rent cpu for 30 days
        let memo = rent_cpu_memo( 'eosio', 30, 1000 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(36), memo]).send('mike@active');               

        //extend
        const action = contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(36), "|extend_rental||"]).send('mike@active'); 
        await expectToThrow(action, "eosio_assert: memo for extend_rental operation is incomplete");
    });        

});

describe('\n\nsend increase_rental memo', () => {

    it('error: rental doesnt exist', async () => {
        const memo = increase_rental_memo( 'mike', 10 ); //receiver, wax
        const action = contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(10), memo]).send('eosio@active'); 
        await expectToThrow(action, ERR_RENTAL_DOESNT_EXIST);
    }); 
      
    it('error: rental expires in less than 1 day', async () => {
        //rent cpu for 30 days
        let memo = rent_cpu_memo( 'eosio', 30, 1000 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(36), memo]).send('mike@active');        
        
        //fast forward time 29.5 days
        await setTime(initial_state.chain_time + (60 * 60 * 24 * 30) + 1 );

        //extend
        memo = increase_rental_memo( 'eosio', 10 ); //receiver, wax
        const action = contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(12), memo]).send('mike@active'); 
        await expectToThrow(action, "eosio_assert: cant increase rental with < 1 day remaining");
    });   

    it('error: rental was never funded', async () => {
        //rent cpu for 30 days
        let memo = rent_cpu_memo( 'eosio', 30, 1000 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');           

        //extend
        memo = increase_rental_memo( 'eosio', 10 ); //receiver, wax
        const action = contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(12), memo]).send('mike@active'); 
        await expectToThrow(action, "eosio_assert: you can't increase a rental if it hasnt been funded yet");
    });   

    it('error: rental already expired', async () => {
        //rent cpu for 30 days
        let memo = rent_cpu_memo( 'eosio', 30, 1000 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(36), memo]).send('mike@active');        
        
        //fast forward time 29.5 days
        await setTime(initial_state.chain_time + (60 * 60 * 24 * 31) + 1 );

        //extend
        memo = increase_rental_memo( 'eosio', 10 ); //receiver, wax
        const action = contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(12), memo]).send('mike@active'); 
        await expectToThrow(action, "eosio_assert: this rental has already expired");
    });    

    it('error: didnt send enough funds', async () => {
        //rent cpu for 30 days
        let memo = rent_cpu_memo( 'eosio', 30, 500 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(50), memo]).send('mike@active');        

        //extend
        memo = increase_rental_memo( 'eosio', 500 ); //receiver, wax
        const action = contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(12), memo]).send('mike@active'); 
        
        //rentals give up to 1 full extra day to the user when they first rent
        //depending on the time of day they rent
        //since the chain time is the beginning of a day right now, the cost to increase is based on 31 days
        //30 day rental + the 24 hours remaining before their rental timer officially starts
        await expectToThrow(action, "eosio_assert: expected to receive 18.60000000 WAX");
    });                
            
    it('error: minimum amount to increase', async () => {
        //rent cpu for 30 days
        let memo = rent_cpu_memo( 'eosio', 30, 500 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(50), memo]).send('mike@active');        

        //extend
        memo = increase_rental_memo( 'eosio', 1 ); //receiver, wax
        const action = contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(12), memo]).send('mike@active'); 
        await expectToThrow(action, "eosio_assert: minimum wax amount to increase is 5.00000000 WAX");
    });     

    it('error: maximum amount to increase', async () => {
        //rent cpu for 30 days
        let memo = rent_cpu_memo( 'eosio', 30, 500 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(50), memo]).send('mike@active');        

        //extend
        memo = increase_rental_memo( 'eosio', 100000001 ); //receiver, wax
        const action = contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(12), memo]).send('mike@active'); 
        await expectToThrow(action, ERR_MAX_AMOUNT_TO_RENT);
    });    

    it('error: incomplete memo', async () => {
        //rent cpu for 30 days
        let memo = rent_cpu_memo( 'eosio', 30, 500 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(50), memo]).send('mike@active');        

        //extend
        memo = '|increase_rental|hi|'; //receiver, wax
        const action = contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(12), memo]).send('mike@active'); 
        await expectToThrow(action, "eosio_assert: memo for increase_rental operation is incomplete");
    });    

    it('successful increase_rental operation', async () => {
        //rent cpu for 30 days
        let memo = rent_cpu_memo( 'eosio', 30, 500 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(50), memo]).send('mike@active');        

        //extend
        memo = increase_rental_memo( 'eosio', 100 );
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(3.72), memo]).send('mike@active'); 
    });                              
});


describe('\n\nclaimgbmvote action', () => {

    it('error: no rewards to claim', async () => {
        const action = contracts.pol_contract.actions.claimgbmvote([]).send('mike@active'); 
        await expectToThrow(action, "eosio_assert: you arent staking anything");
    }); 

    it('error: hasnt been 24 hours', async () => {
        //rent cpu for 30 days
        let memo = rent_cpu_memo( 'eosio', 30, 500 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(50), memo]).send('mike@active');         

        //fast forward 24h
        await setTime(initial_state.chain_time + (60 * 60 * 24) - 1 );

        const action = contracts.pol_contract.actions.claimgbmvote([]).send('mike@active'); 
        await expectToThrow(action, "eosio_assert: hasnt been 24h since your last claim");
    });    
      
    it('successful claim', async () => {
        //rent cpu for 30 days
        let memo = rent_cpu_memo( 'eosio', 30, 500 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(50), memo]).send('mike@active');         

        //fast forward 24h
        await setTime(initial_state.chain_time + (60 * 60 * 24) );

        await contracts.pol_contract.actions.claimgbmvote([]).send('mike@active'); 
    });   
                             
});


describe('\n\nclaimrefund action', () => {

    it('error: no refund to claim', async () => {
        const action = contracts.pol_contract.actions.claimrefund([]).send('mike@active'); 
        await expectToThrow(action, "eosio_assert: refund request not found");
    }); 

    it('error: the request is < 3 days old', async () => {
        //rent cpu for 30 days
        let memo = rent_cpu_memo( 'eosio', 30, 500 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(50), memo]).send('mike@active');         

        //fast forward 31d and 1s
        await setTime(initial_state.chain_time + (60 * 60 * 24 * 31) + 1 );

        //clear expired rentals
        await contracts.pol_contract.actions.clearexpired([500]).send('eosio@active');  
        const action = contracts.pol_contract.actions.claimrefund([]).send('mike@active'); 
        await expectToThrow(action, "eosio_assert: refund is not available yet");
    });    
      
    it('successful claim', async () => {
        //rent cpu for 30 days
        let memo = rent_cpu_memo( 'eosio', 30, 500 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(50), memo]).send('mike@active');         

        //fast forward 31d and 1s
        await setTime(initial_state.chain_time + (60 * 60 * 24 * 31) + 1 );

        //clear expired rentals
        await contracts.pol_contract.actions.clearexpired([500]).send('eosio@active');  

        //fast forward 3 more days and claim      
        await setTime(initial_state.chain_time + (60 * 60 * 24 * 34) + 1 );
        await contracts.pol_contract.actions.claimrefund([]).send('mike@active'); 
    });   
                             
});


describe('\n\nclearexpired action', () => {

    it('error: no expired rentals', async () => {
        const action = contracts.pol_contract.actions.clearexpired([500]).send('eosio@active');  
        await expectToThrow(action, "eosio_assert: no expired rentals to clear");

    }); 
  
    it('error: pending refund > 5 mins && < 3 days old', async () => {
        let current_time = initial_state.chain_time;

        //rent cpu for 30 days to account 1
        let memo = rent_cpu_memo( 'eosio', 30, 500 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(50), memo]).send('mike@active');         

        //fast forward 1d
        current_time += (60 * 60 * 24 * 1);
        await setTime(current_time);

        //rent cpu for 30 days to account 2
        memo = rent_cpu_memo( 'bob', 30, 500 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'bob']).send('mike@active');          
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(50), memo]).send('mike@active');         

        //fast forward 31d and 1s
        current_time += (60 * 60 * 24 * 31) + 1;
        await setTime(current_time);

        //clear expired rental 1
        await contracts.pol_contract.actions.clearexpired([500]).send('eosio@active'); 

        //fast forward 1d
        current_time += (60 * 60 * 24);
        await setTime(current_time);

        //clear expired rental 2
        const action = contracts.pol_contract.actions.clearexpired([500]).send('eosio@active'); 
        await expectToThrow(action, "eosio_assert: there is a pending refund > 5 mins and < 3 days old");

    });   

    it('success: pending refund is < 5 mins old', async () => {
        let current_time = initial_state.chain_time;

        //rent cpu for 30 days to account 1
        let memo = rent_cpu_memo( 'eosio', 30, 500 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(50), memo]).send('mike@active');         

        //fast forward 1d
        current_time += (60 * 60 * 24 * 1);
        await setTime(current_time);

        //rent cpu for 30 days to account 2
        memo = rent_cpu_memo( 'bob', 30, 500 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'bob']).send('mike@active');          
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(50), memo]).send('mike@active');         

        //fast forward 32d and 1s so both rentals expire
        current_time += (60 * 60 * 24 * 32) + 1;
        await setTime(current_time);

        //clear expired rental 1
        await contracts.pol_contract.actions.clearexpired([1]).send('eosio@active'); 

        //fast forward 4m
        current_time += (60 * 4);
        await setTime(current_time);

        //clear expired rental 2
        await contracts.pol_contract.actions.clearexpired([1]).send('eosio@active'); 
    });   

    it('success: pending refund is > 3 days old', async () => {
        let current_time = initial_state.chain_time;

        //rent cpu for 30 days to account 1
        let memo = rent_cpu_memo( 'eosio', 30, 500 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(50), memo]).send('mike@active');         

        //fast forward 1d
        current_time += (60 * 60 * 24 * 1);
        await setTime(current_time);

        //rent cpu for 30 days to account 2
        memo = rent_cpu_memo( 'bob', 30, 500 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'bob']).send('mike@active');          
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(50), memo]).send('mike@active');         

        const rentersBefore = await getRenters();
        assert(rentersBefore.length == 2, "there should be 2 rows in the renters table");

        //fast forward 31d and 1s
        current_time += (60 * 60 * 24 * 31) + 1;
        await setTime(current_time);

        //clear expired rental 1
        await contracts.pol_contract.actions.clearexpired([1]).send('eosio@active'); 
        const rentersBetween = await getRenters();
        assert(rentersBetween.length == 1, "there should be 1 row in the renters table");        

        //fast forward 3d and 1s
        current_time += (60 * 60 * 24 * 3) + 1;
        await setTime(current_time);

        //clear expired rental 2
        await contracts.pol_contract.actions.clearexpired([1]).send('eosio@active'); 
        const rentersAfter = await getRenters();
        assert(rentersAfter.length == 0, "there should be 0 rows in the renters table");        
    });        
          
    it('success: multiple expired requests and no pending refunds', async () => {
        let current_time = initial_state.chain_time;

        //rent cpu for 30 days to account 1
        let memo = rent_cpu_memo( 'eosio', 30, 500 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'eosio']).send('mike@active');          
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(50), memo]).send('mike@active');         

        //rent cpu for 30 days to account 2
        memo = rent_cpu_memo( 'bob', 30, 500 ); //receiver, days, wax
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(1000), 'for staking pool only']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(200), '']).send('eosio@active');  
        await contracts.pol_contract.actions.rentcpu(['mike', 'bob']).send('mike@active');          
        await contracts.wax_contract.actions.transfer(['mike', 'pol.fusion', wax(50), memo]).send('mike@active');         

        //renters table should have 2 rows
        const rentersBefore = await getRenters();
        assert(rentersBefore.length == 2, "there should be 2 rows in the renters table");

        //fast forward 31d and 1s
        current_time += (60 * 60 * 24 * 31) + 1;
        await setTime(current_time);

        //clear both expired rentals
        await contracts.pol_contract.actions.clearexpired([500]).send('eosio@active'); 
        const rentersAfter = await getRenters();
        assert(rentersAfter.length == 0, "there should be 0 rows in the renters table");
    });                                 
});


describe('\n\ndeleterental action', () => {

    it('error: missing auth', async () => {
        await contracts.pol_contract.actions.rentcpu(['eosio', 'mike']).send('eosio@active');  
        const action = contracts.pol_contract.actions.deleterental([0]).send('mike@active');  
        await expectToThrow(action, "missing required authority eosio");
    }); 

    it('error: cant delete funded rental', async () => {
        await contracts.pol_contract.actions.rentcpu(['eosio', 'mike']).send('eosio@active');  
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(12000), 'for staking pool only']).send('eosio@active');

        const memo = rent_cpu_memo('mike', 10, 1000);
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(100), memo]).send('eosio@active');
        const action = contracts.pol_contract.actions.deleterental([0]).send('eosio@active');  
        await expectToThrow(action, "eosio_assert: can not delete a rental after funding it, use the clearexpired action");
        
    });     

    it('success', async () => {
        await contracts.pol_contract.actions.rentcpu(['eosio', 'mike']).send('eosio@active');  
        const renters = await getRenters()
        assert(renters[0].renter == "eosio", "rental 0 should be from eosio")
        await contracts.pol_contract.actions.deleterental([0]).send('eosio@active');  
        const renters_after = await getRenters()
        assert(renters_after.length == 0, "rentals after should be empty")        
    });     
                                  
});


describe('\n\nrebalance action', () => {

    it('error: both buckets are empty', async () => {
        const action = contracts.pol_contract.actions.rebalance([]).send('mike@active');  
        await expectToThrow(action, "eosio_assert: there are no assets to rebalance");
    }); 

    it('error: full buckets but alcor out of range and its been < 7 days', async () => {
        await contracts.alcor_contract.actions.initunittest([wax(10000), lswax(5000)]).send();   
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(100000), '']).send('eosio@active');
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(12000), 'for staking']).send('eosio@active');
        await stake('mike', 12000, true, 6333)
        await contracts.token_contract.actions.transfer(['mike', 'pol.fusion', lswax(6333), '']).send('mike@active');
        const action = contracts.pol_contract.actions.rebalance([]).send('mike@active');  
        await expectToThrow(action, "eosio_assert: no need to rebalance");       
    }); 

    it('success: both buckets filled && alcor in range', async () => {
        //set alcor range to the same as real_price since rebalance buys at real price
        await contracts.alcor_contract.actions.initunittest([wax(100), lswax(100)]).send();   
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(100000), '']).send('eosio@active');
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(12000), 'for staking']).send('eosio@active');
        await stake('mike', 12000, true, 6333)
        await contracts.token_contract.actions.transfer(['mike', 'pol.fusion', lswax(6333), '']).send('mike@active');

        const pol_state = await getPolState();
        assert(pol_state.wax_bucket == wax(100000), "pol wax should be 100k");
        assert(pol_state.lswax_bucket == lswax(6333), "pol lswax should be 6333")        


        await contracts.pol_contract.actions.rebalance([]).send('mike@active');  
        const pol_state_after = await getPolState();
        assert(pol_state_after.wax_bucket == wax(0), "pol should have 0 wax");
        assert(pol_state_after.lswax_bucket == lswax(0), "pol should have 0 lswax");
                     
    });     
    
    it('7 days passed. positive wax bucket, 0 lswax. alcor out of range', async () => {
        let current_time = initial_state.chain_time;
        await contracts.alcor_contract.actions.initunittest([wax(1100), lswax(100)]).send();   
        await contracts.wax_contract.actions.transfer(['eosio', 'pol.fusion', wax(100000), '']).send('eosio@active');
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(12000), 'fugdfj']).send('eosio@active');

        const pol_state = await getPolState();
        assert(pol_state.wax_bucket == wax(100000), "pol wax should be 100k");
        assert(pol_state.lswax_bucket == lswax(0), "pol lswax should be 0")        

        //fast forward 7 days
        current_time += (60 * 60 * 24 * 7) + 1
        await setTime(current_time)
        await contracts.pol_contract.actions.rebalance([]).send('mike@active');  
        const pol_state_after = await getPolState();
        assert(pol_state_after.wax_available_for_rentals == wax(100000), "rental bucket should be 100k");           
    });     

    it('7 days passed. positive lswax bucket, 0 wax. alcor out of range', async () => {
        let current_time = initial_state.chain_time;
        await contracts.alcor_contract.actions.initunittest([wax(1100), lswax(100)]).send();   
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(120000), 'fugdfj']).send('eosio@active');
        await stake('mike', 120000, true, 50000)
        await contracts.token_contract.actions.transfer(['mike', 'pol.fusion', lswax(50000), '']).send('mike@active');

        const pol_state = await getPolState();
        assert(pol_state.wax_bucket == wax(0), "pol wax should be 0");
        assert(pol_state.lswax_bucket == lswax(50000), "pol lswax should be 50k")        

        //fast forward 7 days, account for the 0.05% instant redeem fee
        current_time += (60 * 60 * 24 * 7) + 1
        await setTime(current_time)
        await contracts.pol_contract.actions.rebalance([]).send('mike@active');  
        await getDappState()
        const pol_state_after = await getPolState();
        const expected_outcome = wax(50000 * 0.9995)
        assert(pol_state_after.wax_available_for_rentals == `${expected_outcome}`, `rental bucket should be ${expected_outcome}`);
                     
    });      

    it('error: 7 days passed. positive lswax bucket, 0 wax. alcor in range, but dapp has no instant redeem bucket', async () => {
        let current_time = initial_state.chain_time;
        await contracts.alcor_contract.actions.initunittest([wax(1000), lswax(1000)]).send();   
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(120000), 'fugdfj']).send('eosio@active');
        await stake('mike', 120000, true, 50000)
        await contracts.token_contract.actions.transfer(['mike', 'pol.fusion', lswax(50000), '']).send('mike@active');

        const pol_state = await getPolState();
        assert(pol_state.wax_bucket == wax(0), "pol wax should be 0");
        assert(pol_state.lswax_bucket == lswax(50000), "pol lswax should be 50k")  

        //fast forward 1 day, stakeallcpu on dapp to use up the instant redeem funds
        current_time += (60 * 60 * 24 * 1) + 1
        await setTime(current_time);
        await contracts.dapp_contract.actions.stakeallcpu([]).send('mike@active');
        const dapp_state = await getDappState()
        await assert(dapp_state.wax_available_for_rentals == wax(0), "dapp should have 0 wax for rentals");

        //fast forward 7 days, account for the 0.05% instant redeem fee
        current_time += (60 * 60 * 24 * 7) + 1
        await setTime(current_time)
        const action = contracts.pol_contract.actions.rebalance([]).send('mike@active');  
        await expectToThrow(action, "eosio_assert: can not rebalance because we're unable to instant redeem lswax for wax");
    });    

    it('success: 7 days passed. positive lswax bucket, 0 wax. alcor in range, dapp has wax', async () => {
        let current_time = initial_state.chain_time;
        await contracts.alcor_contract.actions.initunittest([wax(1000), lswax(1000)]).send();   
        await contracts.wax_contract.actions.transfer(['eosio', 'mike', wax(120000), 'fugdfj']).send('eosio@active');
        await stake('mike', 120000, true, 50000)
        await contracts.token_contract.actions.transfer(['mike', 'pol.fusion', lswax(50000), '']).send('mike@active');

        const pol_state = await getPolState();
        assert(pol_state.wax_bucket == wax(0), "pol wax should be 0");
        assert(pol_state.lswax_bucket == lswax(50000), "pol lswax should be 50k")  

        //fast forward 7 days, account for the 0.05% instant redeem fee
        current_time += (60 * 60 * 24 * 7) + 1
        await setTime(current_time)
        await contracts.pol_contract.actions.rebalance([]).send('mike@active');  

        //50k lswax - 0.05% fee = 49,975 / 2 = 24,987.5 of each asset
        const expected_amount = parseFloat( (50000 * 0.9995) / 2 + 1000 ).toFixed(8)
        const alcor_state = await getAlcorPool()
        await assert(alcor_state.tokenA.quantity == `${expected_amount} WAX`, `alcor should have ${expected_amount} WAX`)
        await assert(alcor_state.tokenB.quantity == `${expected_amount} LSWAX`, `alcor should have ${expected_amount} LSWAX`)
    });      
});


describe('\n\nrentcpu action', () => {

    it('error: !is_account( cpu_receiver )', async () => {
        const action = contracts.pol_contract.actions.rentcpu(['eosio', 'billy']).send('eosio@active');  
        await expectToThrow(action, "eosio_assert: billy is not a valid account");

    }); 
                                  
});


describe('\n\nsetallocs action', () => {

    it('error: < minimum allocation', async () => {
        const action = contracts.pol_contract.actions.setallocs([999999]).send('pol.fusion@active');  
        await expectToThrow(action, "eosio_assert: percent must be between > 1e6 && <= 100 * 1e6");
    }); 

    it('error: > maximum allocation', async () => {
        const action = contracts.pol_contract.actions.setallocs([100000001]).send('pol.fusion@active');  
        await expectToThrow(action, "eosio_assert: percent must be between > 1e6 && <= 100 * 1e6");
    }); 

    it('success: 1% liquidity and 99% rental pool', async () => {
        await contracts.pol_contract.actions.setallocs([1000000]).send('pol.fusion@active');  
        const pol_config = await getPolConfig()
        assert(pol_config.liquidity_allocation_1e6 == 1000000, "liquidity_allocation should be 100000");
        assert(pol_config.rental_pool_allocation_1e6 == 100000000 - 1000000, "liquidity_allocation should be 99%")
    });    

    it('success: 100% liquidity and 0% rental pool', async () => {
        await contracts.pol_contract.actions.setallocs([100000000]).send('pol.fusion@active');  
        const pol_config = await getPolConfig()
        assert(pol_config.liquidity_allocation_1e6 == 100000000, "liquidity_allocation should be 100000");
        assert(pol_config.rental_pool_allocation_1e6 == 0, "liquidity_allocation should be 0%")
    });     
});