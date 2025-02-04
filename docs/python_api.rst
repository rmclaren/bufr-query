.. _bufr-python-api:
:tocdepth: 3

Python
======

High Level API
--------------

The high level API allows you to make use of YAML files to define the queries and encoded mappings. This makes it
convenient to use in conjunction with the "script" interface on ioda when you discover that special processing is needed
to get the data into the correct format. In general this interface will also be faster, as the bulk of the
work will be done in native code. Here is a simple example in the form of a "script" backend script:

.. code-block:: python

  import bufr
  from bufr.encoders import netcdf

  def get_data(input_path, category):
    YAML_PATH = 'testinput/bufrtest_hrs_basic_mapping.yaml'
    OUTPUT_PATH = 'testrun/hrs.nc'

    container = bufr.Parser(input_path, YAML_PATH).parse()

    datasets = netcdf.Encoder(YAML_PATH).encode(container, OUTPUT_PATH)[category]
    obs_temp = dataset["ObsValue/brightnessTemperature"][:]
    dataset.close()

    return obs_temp

As opposed to dealing with the low level components (bufr.QuerySet, bufr.File, bufr.ResultSet) directly the example
makes use of the bufr.Parser, bufr.DataContainer, and bufr.encoder.netcdf.Encoder components. The
:ref:`Mapping YAML File` defines the queries and mapping to the resulting ObsGroup object in its **bufr** and
**encoder** sections.

The basic steps involved are:
    #. Import the bufr module
    #. Create a Parser object with the path to the BUFR file and the path to the YAML file
    #. Parse the BUFR file into a DataContainer object
    #. *(optional)* Modify the DataContainer
    #. Create an Encoder object with the path to the mapping YAML file
    #. Encode the DataContainer object as a dict (keys=tuple(str) and values=ObsGroup)
    #. Return the encoded ObsGroup object

DataContainer
~~~~~~~~~~~~~

The DataContainer contains all the data parsed from a BUFR file organized into Categories. The mapping file makes it
possible to define "splits". For example you might want to split the data set according to satellite ID so each
configured satellites data ends up in a separate ObsSpace. Each split element is a "Category". Actually you could take
a dataset and split it according multiple variables. So each "Category" a set of splits. So for example: I could split
the data according to satellite ID and channel number. The resulting DataContainer would have a Category for each
possible combination of satellite ID and channel number. The categories are uniquely identified by the combination of
the satellite ID and channel number. This is why category identifiers are lists of strings. These are important to keep
in mind if you wish to modify the DataContainer before encoding it (if there are no splits defined there is only 1
category).

Here is what the DataContainer looks like:

.. class:: DataContainer

      .. method:: get(field_name, category_id=[])

          Get path names for a field.

      .. method:: add(field_name, py_data, dim_paths, category_id=[])

          Add a new variable object into the data container.

      .. method:: get_paths(field_name, category_id=[])

          Adds a new Category object to the DataContainer with the given category_id.

      .. method:: replace(field_name, py_data, category_id=[])

          Replace the variable with the given name.

      .. method:: get_category_map()

          Get the map of categories.

      .. method:: all_sub_categories()

          Get a list of all the subcategories.

      .. method:: list()

          Get the names of all the variable fields.

      .. method:: append(other)

          Append the other DataContainer to this one.

      .. method:: gather(comm)

          Gather the DataContainer data from all the ranks.


So to replace a value in the DataContainer you would do something like this (assuming only 1 category):

.. code-block:: python

  import bufr
  from bufr.encoders import netcdf

  def get_data(input_path):
    YAML_PATH = 'testinput/bufrtest_hrs_basic_mapping.yaml'
    OUTPUT_PATH = 'testrun/hrs.nc'

    container = bufr.Parser(input_path, YAML_PATH).parse()

    data = container.get('variables/brightnessTemp')
    container.replace('variables/brightnessTemp', data * 1.1)

    datasets = netcdf.Encoder(YAML_PATH).encode(container, OUTPUT_PATH).values()
    obs_temp = dataset["ObsValue/brightnessTemperature"][:]
    dataset.close()

    return obs_temp

Encoder Description
~~~~~~~~~~~~~~~~~~~

Taking this a step further, adding a new variable requires that you also add the variable to the Description so
that the Encoder writes it out to the ObsGroup.

.. class:: Description

      .. method:: add_variable(field_name, dim_paths, units, long_name='')

          Add a new variable object to the output description

      .. method:: remove_variable(field_name)

          Remove a variable object from the output description

      .. method:: add_dimension(dim_name, paths, source='')

          Add a new dimension object to the output description

      .. method:: remove_dimension(dim_name)

            Remove a dimension object from the output description

      .. method:: add_global(global_name, value)

          Add a new global object to the output description

      .. method:: remove_global(global_name)

            Remove a global object from the output description


So the code looks more like this:

.. code-block:: python

  import bufr
  from bufr.encoders import netcdf

  def get_data(input_path):
      YAML_PATH = 'testinput/bufrtest_hrs_basic_mapping.yaml'
      OUTPUT_PATH = 'testrun/hrs.nc'

      container = bufr.Parser(input_path, YAML_PATH).parse()

      data = container.get('variables/brightnessTemp')
      paths = container.get_paths('variables/brightnessTemp')
      container.add('variables/brightnessTemp_new', data*.01, paths)

      description = netcdf.Description(YAML_PATH)
      description.add_variable(name='ObsValue/new_brightnessTemperature',
                               source='variables/brightnessTemp_new',
                               units='K',
                               longName='New Brightness Temperature')

      dataset = next(iter(netcdf.Encoder(description).encode(container, OUTPUT_PATH).values()))
      return dataset


Adding a new variable is a little more involved. The most difficult part is to correctly configure a path for the
variable. The easiest way to solve this is to copy the path from an existing variable otherwise you will have to
think very carefully.

MPI
~~~

It is possible to parse a BUFR file in parallel using MPI. The Parser and DataContainer have methods
to make this easy. Please see the following example:

.. code-block:: python

  import bufr
  from bufr.encoders import netcdf

  def mpi_example():
      DATA_PATH = 'testinput/data/gdas.t18z.1bmhs.tm00.bufr_d'
      YAML_PATH = 'testinput/bufrtest_mhs_basic_mapping.yaml'
      OUTPUT_PATH = 'testrun/mhs_basic_parallel.nc'

      bufr.mpi.App(sys.argv)  # Initialize the MPI application (not needed if ioda script)
      comm = bufr.mpi.Comm("world")  # Get the MPI communicator
      container = bufr.Parser(DATA_PATH, YAML_PATH).parse(comm)  # Parse the BUFR file with mpi
      container.gather(comm)  # (OPTIONAL) Gather the DataContainer data from all the ranks

      if comm.rank() == 0:
          netcdf.Encoder(YAML_PATH).encode(container, OUTPUT_PATH) # Encode the DataContainer object

Please note that gathering the DataContainer data is optional. If you wanted to see the data from
each rank you could skip the gather step and write out the data from each rank to a separate file.

DataCache
~~~~~~~~~

Sometimes you may want to read a Bufr file once, and then reuse the result for mulitple ObsSpaces (reading BUFR is time
consuming). The DataCache class makes this possible by providing a singleton that can be used to cache the read
BUFR data.

.. class:: DataCache

      .. method:: has(src_path, map_path)

          Does the cache contain the given data?

      .. method:: add(src_path, map_path, cache_categories, data_container)

          Add a new data container to the cache. Include cache_categories list[list[str]] that we plan to read.

      .. method:: get(src_path, map_path)

          Get the data container for the given src_path and map_path.

      .. method:: mark_finished(src_path, map_path, category)

          Mark the given category as finished. Once all the cache_categories are finished the data container will be
          removed from the cache.

Example:

.. code-block:: python

  import bufr
  from bufr.encoders import netcdf

  def get_data(input_path, category):
      YAML_PATH = 'testinput/bufr_hrs.yaml'
      OUTPUT_PATH = 'testrun/hrs.nc'

      if not bufr.DataCache.has(input_path, YAML_PATH):
        container = bufr.Parser(input_path, YAML_PATH).parse()
        bufr.DataCache.add(input_path, YAML_PATH, dat.allSubCategories(), dat)
      else:
        container = bufr.DataCache.get(input_path, YAML_PATH)
      bufr.DataCache.mark_finished(input_path, YAML_PATH, category)

      data = container.get('variables/brightnessTemp', category)
      container.replace('variables/brightnessTemp', data*.01, category)

      dataset = netcdf.Encoder(YAML_PATH).encode(container, OUTPUT_PATH)[category]
      return dataset



Low Level API
-------------

The low level python API allows you to read BUFR files using pure python without the need to create any
yaml files.

.. code-block:: python

    import bufr

    # Make the QuerySet for all the data we want
    q = bufr.QuerySet()
    q.add('latitude', '*/CLAT')
    q.add('longitude', '*/CLON')
    q.add('radiance', '*/BRIT/TMBR')

    # Open the BUFR file and execute the QuerySet
    with bufr.File( './testinput/gdas.t00z.1bhrs4.tm00.bufr_d') as f:
        r = f.execute(q)

    # Use the ResultSet returned to get correctly dimensioned numpy arrays of the data
    lat = r.get('latitude')
    lon = r.get('longitude')
    rad = r.get('radiance')

The steps are:
    #. Import bufr
    #. Create a QuerySet
    #. Open the bufr file (using the with statement)
    #. Execute the QuerySet
    #. Use the ResultSet to get the data

Create a QuerySet
~~~~~~~~~~~~~~~~~

The QuerySet is a list of queries that you want to execute on the BUFR file. To create one, just
create an instance of the QuerySet class and then add queries to it using the `add` method. Each
item in the QuerySet consists of a name and the corresponding query path. The name is used to
retrieve the data from the ResultSet. It can be anything you want! The path can be any query path
described in :ref:`Query Path`.

If you are only interested in specific subsets within the BUFR file you can instantiate the QuerySet
with a list of the Subsets you want. For example:

.. code-block:: python

    # Make the QuerySet for all the data we want
    q = bufr.QuerySet(['NC000001', 'NC000002'])
    q.add('latitude', '*/CLAT')
    q.add('longitude', '*/CLON')
    q.add('radiance', '*/BRIT/TMBR')

    # And so on...

Execute the QuerySet
~~~~~~~~~~~~~~~~~~~~

Just open the BUFR file and run execute on on the File object with the query set. It will run
through the entire BUFR file and return a ResultSet object.


Use the ResultSet
~~~~~~~~~~~~~~~~~

Internally the ResultSet contains data structures which allow it to construct the numpy array data
sets using the keys defined in the QuerySet. To get the data, just use the `get` method. The data returned
will have the shape of the data in the BUFR file (ex: rad.shape from above will be (num_locations, num_channels)).

It is also possible to group data elements with respect to each other. In this case call `get` with
the field you want to group by (see :ref:`Result Set`). So for example:

.. code-block:: python

    lat_grouped = r.get('latitude', group_by='radiance')
    lon_grouped = r.get('longitude', group_by='radiance')
    rad_grouped = r.get('radiance', group_by='radiance')

Applying the group_by field will have the effect of flattening the data (rad_grouped.shape will be 1 dimensional
(num_locations * num_channels)). The lat lon values will be repeated for each channel so each "row" will be associated
with the correct coordinate values.

The result in either case are `masked numpy arrays <https://numpy.org/doc/stable/reference/maskedarray.generic.html>`_.
