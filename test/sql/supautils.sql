-- build extension
create schema supa;
create extension supautils with schema supa;

select supa.get_one();
