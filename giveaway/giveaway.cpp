#include <nftgiveaway.hpp>
/*
NFT Giveaway contrak.

Users send 0.1 EOS to the contract to be enrolled in the giveaway.
*/

void nftgiveaway::setlocked(bool locked)
{ /* This function lets us set the 'locked' variable true or false to determine if people can still send EOS and be enrolled in the giveaway */
    require_auth(get_self());

    totaltable global(get_self(), "totals"_n.value); 
    auto total_it = global.find("totals"_n.value);
    if (total_it == global.end())
    {
        global.emplace(get_self(), [&]( auto& row) 
        {
            row.key    = "totals"_n;
            row.locked = locked;
        });
    }
    else
    {
        global.modify(total_it, get_self(), [&]( auto& row) 
        {
            row.locked = locked;
        });
    }
}

[[::on_notify("eosio.token::transfer")]]
void nftgiveaway::registergiveaway(const name& owner_account, const name& to, const asset& amount_eos_sent, std::string memo)
{
    check(amount_eos_sent.symbol == eos_symbol, "error: these are not the droids you are looking for.");
    if (to != get_self() || owner_account == get_self())
    {
        print("error: these are not the droids you are looking for.");
        return;
    }

    totaltable global(get_self(), "totals"_n.value); 
    auto total_it = global.find("totals"_n.value);
    check(total_it != global.end(), "error: global table is not initiated.");

    check(total_it->locked == false, "error: the giveaway registration has ended. winners will be decided soon!");

    uint16_t giveaway_entry_price = 1000; // 0.1 EOS

    check(amount_eos_sent.amount >= giveaway_entry_price, "error: you must send 0.1 EOS to participate in the giveaway.");

    playertable players(get_self(), get_self().value);
    auto players_it = players.find(owner_account.value);
    check(players_it == players.end(), "error: account has already been registered in the giveaway."); 

    // Update the table(s):
    uint32_t now = current_time_point().sec_since_epoch();
    players.emplace(get_self(), [&](auto& row) 
    {
        row.owner_account = owner_account;
        row.seed = now;
    });

    uint32_t refund_amount = amount_eos_sent.amount - giveaway_entry_price;

    eosio::print_f("Refund for user: [%]\n",refund_amount);
    if (refund_amount > 0)
    {
        asset refund_asset = amount_eos_sent;
        refund_asset.amount = refund_amount;
        nftgiveaway::inline_transfereos(get_self(), owner_account, refund_asset, "Sending refund for the NFT giveaway entry.");
    }
}

void nftgiveaway::refund()
{
    require_auth(get_self());

    playertable players(get_self(), get_self().value);

    asset refund;
    refund.amount = 1000;
    refund.symbol = symbol("EOS", 4);

    auto current_itr = players.begin();
    auto end_itr     = players.end();

    while (current_itr != end_itr)
    {
        eosio::print_f("Refunding: [%]\n",current_itr->owner_account);
        nftgiveaway::inline_transfereos(get_self(), current_itr->owner_account, refund, "Sending refund for the NFT giveaway entry.");
        ++ current_itr;
    }
}

void nftgiveaway::getwinners()
{// To make everything as random as possible, we'll have to do a telegram countdown and involve the users from the channel in determining the moment this function is called.
    require_auth(get_self());

    uint8_t  winner_limit = 25;
    uint16_t player_count = 0;

    vector<uint16_t> winners;
    vector<uint64_t> seeds;

    playertable players(get_self(), get_self().value);

    auto current_itr = players.begin();
    auto end_itr     = players.end();

    while (current_itr != end_itr)
    {
        eosio::print_f("Checking seed from: [%]\n",current_itr->owner_account);
        seeds.push_back(current_itr->seed);
        ++ player_count;
        ++ current_itr;
    }

    eosio::print_f("Finished loop.\n");
    eosio::print_f("We have: [%] max players\n",player_count);
    eosio::print_f("Seeds Vector size: [%]\n",size(seeds));

    bool flag = false;

    uint32_t now = current_time_point().sec_since_epoch();

    for (auto it = seeds.begin(); it != seeds.end(); it++)
    {
        uint16_t newposition = (now + *it)%size(seeds);

        for (auto winner_it = winners.begin(); winner_it != winners.end(); ++winner_it)
        {
            if (newposition == *winner_it)
            {
                flag = true;
                break;
            }
        }

        if (flag == true)
        {
            flag = false;
            continue;
        }

        winners.push_back(newposition);

        if (size(winners) >= winner_limit+20) /* We'll do 20 extra, to account for any duplicates that we might find. */
            break;
    }

    totaltable global(get_self(), "totals"_n.value); 
    auto total_it = global.find("totals"_n.value);
    check(total_it != global.end(), "error: global table is not initiated.");

    global.modify(total_it, get_self(), [&]( auto& row) 
    {
        row.winners = winners;
    });

    for (auto winner_it = winners.begin(); winner_it != winners.end(); ++winner_it)
    {
        eosio::print_f("winnerposition: [%]\n",*winner_it);
    }
}

/* Debug function */
void nftgiveaway::populate(const name& owner_account)
{
    require_auth(get_self());

    playertable players(get_self(), get_self().value);
    auto players_it = players.find(owner_account.value);
    check(players_it == players.end(), "error: account has already been registered in the giveaway."); 

    uint32_t now = current_time_point().sec_since_epoch();
    players.emplace(get_self(), [&](auto& row) 
    {
        row.owner_account = owner_account;
        row.seed = now;
    });
}