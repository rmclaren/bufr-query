# (C) Copyright 2023 NOAA/NWS/NCEP/EMC
import sys

import bufr
from bufr.encoders import netcdf
import numpy as np


def test_basic_query():
    DATA_PATH = 'testdata/gdas.t00z.1bhrs4.tm00.bufr_d'

    # Make the QuerySet for all the data we want
    q = bufr.QuerySet()
    q.add('year', '*/YEAR')
    q.add('month', '*/MNTH')
    q.add('day', '*/DAYS')
    q.add('hour', '*/HOUR')
    q.add('minute', '*/MINU')
    q.add('second', '*/SECO')
    q.add('latitude', '*/CLON')
    q.add('longitude', '*/CLAT')
    q.add('radiance', '*/BRIT{1}/TMBR')
    q.add('radiance_all', '*/BRIT/TMBR')

    # Open the BUFR file and execute the QuerySet
    with bufr.File(DATA_PATH) as f:
        r = f.execute(q)


    # Use the ResultSet returned to get numpy arrays of the data
    lat = r.get('latitude')
    rad = r.get('radiance')
    rad_all = r.get('radiance_all')

    # Validate the values that were returned
    assert np.allclose(lat[0:3], np.array([166.7977, 166.4078, 165.992]))
    assert np.allclose(rad[0:3], np.array([198.69, 254.06, 233.85]))
    assert len(rad_all.shape) == 2

    datetimes = r.get_datetime('year', 'month', 'day', 'hour', 'minute', 'second')
    assert datetimes[5] == np.datetime64('2020-10-26T21:00:01')
    assert datetimes.fill_value == np.datetime64('1970-01-01T00:00:00')


def test_string_field():
    DATA_PATH = 'testdata/gdas.t12z.adpupa.tm00.bufr_d'

    # Make the QuerySet for all the data we want
    q = bufr.QuerySet()
    q.add('borg', '*/BID/BORG')

    # Open the BUFR file and execute the QuerySet
    with bufr.File(DATA_PATH) as f:
        r = f.execute(q)

    # Use the ResultSet returned to get numpy arrays of the data
    borg = r.get('borg')

    # Validate the values that were returned
    assert (np.all(borg[0][0:3] == ['KWBC', 'KWBC', 'KAWN']))


def test_long_str_field():
    DATA_PATH ='testdata/gdas.t06z.snocvr.tm00.bufr_d'

    # Make the QuerySet for all the data we want
    q = bufr.QuerySet()
    q.add('lid', '*/WGOSLID')

    # Open the BUFR file and execute the QuerySet
    with bufr.File(DATA_PATH) as f:
        r = f.execute(q)

    # Use the ResultSet returned to get numpy arrays of the data
    lid = r.get('lid')

    # Validate the values that were returned
    assert (lid[0] == '570282')
    assert (lid[6] == '613180')
    assert (np.all(lid[0:7].mask == [False, True, True, True, True, True, False]))

def test_type_override():
    DATA_PATH = 'testdata/gdas.t00z.1bhrs4.tm00.bufr_d'

    # Make the QuerySet for all the data we want
    q = bufr.QuerySet()
    q.add('day', '*/DAYS')
    q.add('longitude', '*/CLAT')

    # Open the BUFR file and execute the QuerySet
    with bufr.File(DATA_PATH) as f:
        r = f.execute(q)

    day = r.get('day')
    day_float = r.get('day', type='float')

    assert day.dtype == 'int32'
    assert day_float.dtype == 'float32'

    lat = r.get('longitude')
    lat_int = r.get('longitude', type='int')

    assert lat.dtype == 'float32'
    assert lat_int.dtype == 'int32'
    assert lat_int.fill_value == 2147483647  # the max int32 value

def test_invalid_query():
    q = bufr.QuerySet()

    try:
        q.add('latitude', '!!! */CLON')
    except Exception as e:
        return

    assert False, "Did not throw exception for invalid query."


def test_highlevel_replace():
    DATA_PATH = 'testdata/gdas.t00z.1bhrs4.tm00.bufr_d'
    YAML_PATH = 'testinput/bufrtest_hrs_basic_mapping.yaml'
    OUTPUT_PATH = 'testrun/bufrtest_python_test.nc'

    container = bufr.Parser(DATA_PATH, YAML_PATH).parse()

    data = container.get('variables/brightnessTemp')
    container.replace('variables/brightnessTemp', data * 1.1)

    dataset = next(iter(netcdf.Encoder(YAML_PATH).encode(container, OUTPUT_PATH).values()))
    obs_temp = dataset["ObsValue/brightnessTemperature"][:]
    dataset.close()

    assert obs_temp.shape == data.shape
    assert np.allclose(obs_temp[:, :], data * 1.1)

def test_highlevel_add():
    DATA_PATH = 'testdata/gdas.t00z.1bhrs4.tm00.bufr_d'
    YAML_PATH = 'testinput/bufrtest_hrs_basic_mapping.yaml'
    OUTPUT_PATH = 'testrun/bufrtest_python_test.nc'

    container = bufr.Parser(DATA_PATH, YAML_PATH).parse()

    data = container.get('variables/brightnessTemp')
    paths = container.get_paths('variables/brightnessTemp')
    container.add('variables/brightnessTemp_new', data, paths)

    str_data = np.array(["hello"]*data.shape[0])
    container.add('variables/str_data', str_data, container.get_paths('variables/latitude'))

    description = bufr.encoders.Description(YAML_PATH)
    description.add_variable(name='ObsValue/new_brightnessTemperature',
                             source='variables/brightnessTemp_new',
                             units='K',
                             longName='New Brightness Temperature')

    description.add_variable(name='ObsValue/str_data',
                             source='variables/str_data',
                             units='strs',
                             longName='Hello Strings')

    dataset = next(iter(netcdf.Encoder(description).encode(container, OUTPUT_PATH).values()))
    obs_orig = dataset["ObsValue/brightnessTemperature"][:]
    obs_temp = dataset["ObsValue/new_brightnessTemperature"][:]
    obs_strs = dataset["ObsValue/str_data"][:]
    dataset.close()

    assert np.allclose(obs_temp, obs_orig)
    assert obs_temp.shape == data.shape
    assert np.all(obs_strs == str_data)

def test_highlevel_append():
    DATA_PATH = 'testdata/gdas.t00z.1bhrs4.tm00.bufr_d'
    YAML_PATH = 'testinput/bufrtest_hrs_basic_mapping.yaml'

    container = bufr.Parser(DATA_PATH, YAML_PATH).parse()


    new_container = bufr.DataContainer()
    new_container.append(container)
    new_container.append(container)

    data = new_container.get('variables/brightnessTemp')

    orig_data = container.get('variables/brightnessTemp')
    orig_data = np.concatenate((orig_data, orig_data))

    assert orig_data.shape == data.shape
    assert np.allclose(orig_data, data)

def test_highlevel_w_category():
    DATA_PATH = 'testdata/gdas.t12z.1bamua.tm00.bufr_d'
    YAML_PATH = 'testinput/bufrtest_amua_ta_mapping.yaml'
    OUTPUT_PATH = 'testrun/bufrtest_python_test_{splits/satId}.nc'

    container = bufr.Parser(DATA_PATH, YAML_PATH).parse()

    categories = container.all_sub_categories()
    for category in categories:  # [metop-a]
        data = container.get('variables/antennaTemperature', category)
        paths = container.get_paths('variables/antennaTemperature', category)
        container.add('variables/antennaTemperature1', data, paths, category)

    description = bufr.encoders.Description(YAML_PATH)
    description.add_variable(name='ObsValue/brightnessTemperature_new',
                             source='variables/antennaTemperature1',
                             units='K')

    for (key, dataset) in netcdf.Encoder(description).encode(container, OUTPUT_PATH).items():
        obs_orig = dataset["ObsValue/brightnessTemperature"][:]
        obs_new = dataset["ObsValue/brightnessTemperature_new"][:]
        dataset.close()

        assert np.allclose(obs_orig, obs_new)

def test_highlevel_cache():
    DATA_PATH = 'testdata/gdas.t12z.1bamua.tm00.bufr_d'
    YAML_PATH = 'testinput/bufrtest_amua_ta_mapping.yaml'

    if not bufr.DataCache.has(DATA_PATH, YAML_PATH):
        dat = bufr.Parser(DATA_PATH, YAML_PATH).parse()
        bufr.DataCache.add(DATA_PATH, YAML_PATH, dat.all_sub_categories(), dat)
    else:
        assert False, "Data Cache incorrect."

    if not bufr.DataCache.has(DATA_PATH, YAML_PATH):
        assert False, "Data Cache does not contain entry."

    container = bufr.DataCache.get(DATA_PATH, YAML_PATH)

    categories = container.all_sub_categories()
    for category in categories:
        cache_dat = bufr.DataCache.get(DATA_PATH, YAML_PATH)
        assert np.allclose(dat.get('variables/antennaTemperature', category),
                           cache_dat.get('variables/antennaTemperature', category))

        bufr.DataCache.mark_finished(DATA_PATH, YAML_PATH, category)

    if bufr.DataCache.has(DATA_PATH, YAML_PATH):
        assert False, "Data Cache still contains entry."


if __name__ == '__main__':
    # Low level interface tests
    test_basic_query()
    test_string_field()
    test_long_str_field()
    test_type_override()
    test_invalid_query()

    # High level interface tests
    test_highlevel_replace()
    test_highlevel_add()
    test_highlevel_w_category()
    test_highlevel_cache()
    test_highlevel_append()

