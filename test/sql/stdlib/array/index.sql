select supa.index(array['a','b','c'], 'c');

select supa.index(array[1, 2, 3], 4);

select supa.index(array[]::int[], 1);

select supa.index(null::int[], 1);
