\echo Execute "CREATE EXTENSION pg_int_split;" to use this extension. \quit

CREATE FUNCTION int_split(int4, int4) RETURNS int4
	LANGUAGE c WINDOW STABLE PARALLEL SAFE
	AS 'MODULE_PATHNAME', 'window_int_split_32';

CREATE FUNCTION int_split(int8, int8) RETURNS int8
	LANGUAGE c WINDOW STABLE PARALLEL SAFE
	AS 'MODULE_PATHNAME', 'window_int_split_64';
