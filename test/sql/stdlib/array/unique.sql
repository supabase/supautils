select supa.unique(array['a', 'a', 'b', 'a']);

select supa.unique(array[1, 2, 3, 2]);

select supa.unique(array[]::int[]);

select supa.unique(null::int[]);
