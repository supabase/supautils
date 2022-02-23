-- placeholder non-restricted values pass
select set_config('response.headers', '[{"Cache-Control": "public"}, {"Cache-Control": "max-age=259200"}]', true);

-- placeholder restricted values fail on a set_config
select set_config('response.headers', '[{"Content-Type": "application/vnd.my.malicious.type"}, {"Cache-Control": "public"}]', true);
select set_config('response.headers', '[{"X-Special-Header": "a-value"}]', true);
\echo

-- placeholder restricted values fail on a SET
set another.placeholder to 'special-value';
