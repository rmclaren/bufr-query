.. _bufr-yaml:

Mapping YAML File
=================

The YAML mapping tells the BUFR component fields to read from the BUFR file, and how
to encode those fields into the output format. To do that it defines 2 sections: `bufr` and
`encoder`. The content of these sections (described bellow) can be thought of as descriptions for what
will be read and how it will be encoded.

.. note::
  Please see the test/testinput directory for examples.

BUFR Description
~~~~~~~~~~~~~~~~

This section describes the BUFR parameters that will be read form the BUFR file, and allows
for some basic operations to read the data in a form that is useful. For example, it has the
ability to group variables in order to "unroll" sections of the BUFR file data. You can split BUFR
data into sub groups (categories) based on the value of a BUFR field (for example: you can categorize
data based on satellite IDs). You can also filter data based on the value of a BUFR field.

Here is an example:

.. code-block:: yaml

  bufr:
    group_by_variable: longitude  # Optional
    subsets:
      - NC004001
      - NC004002
      - NC004003
    variables:
      timestamp:
        datetime:
          year: "*/YEAR"
          month: "*/MNTH"
          day: "*/DAYS"
          hour: "*/HOUR"
          minute: "*/MINU"
          second: "*/SECO"  # default assumed zero if skipped or found as missing
          hoursFromUtc: 0  # Optional

      # Or, sometimes BUFR data use an offset time related to model analysis/cycle.
      timestamp:
        timeoffset:
          timeOffset: "*/PRSLEVEL/DRFTINFO/HRDR"
          transforms:
            - scale: 3600
          referenceTime: "2020-11-01T12:00:00Z"

      satellite_id:
        query: "*/SAID"
        type: int64
      longitude:
        query: "*/CLON"
        transforms:
          - offset: -180
      latitude:
        query: "*/CLAT"
      channels:
        query: "[*/BRITCSTC/CHNM, */BRIT/CHNM]"
      radiance:
        query: "[*/BRITCSTC/TMBR, */BRIT/TMBR]"

    splits:
      satId:
        category:
          variable: satellite_id
          map:
            _3: sat_1  # can't use integers as keys
            _5: sat_2
            _8: sat_3

    filters:
      - bounding:
          variable: longitude
          upperBound: -68  # optional
          lowerBound: -86.3  # optional

The bufr section contains a section called **exports** which defines the data to read from the BUFR.
It has the following sub-sections:

* **group_by_variable**: *(optional)* String value that defines the name of the variable to group
  observations by. If this field is missing then observations will not be re-grouped.
* **subsets**: *(optional)* List of subsets that you want to process. If the field is not present then
  all subsets will be processed in accordance with the query definitions.
* **variables**: List of variables to read as key value pairs.

  * **keys** are arbitrary strings (anything you want). They can be referenced in the output section.
  * **values** (One of these types):

    * **query**: Query string which is used to get the data from the BUFR file. *(optional)* Can
      apply a list of **tranforms** to the numeric (not string) data. Possible transforms are
      **offset** and **scale**. You can also manually override the type by specifying the **type** as
      **int**, **int64**, **float**, or **double**.
    * **datetime**: Associate **key** with data for mnemonics for **year**, **month**, **day**, **hour**,
      **minute**, *(optional)* **second**, and *(optional)* **hoursFromUtc** (must be an **integer**).
      Internally, the value stored is number of seconds elapsed since a reference epoch, currently
      set to 1970-01-01T00:00:00Z.
    * **timeoffset**: Associate **key** with data for mnemonic for **timeOffset**, that should result
      in seconds relative to an ISO-8601 string of date and time (e.g., `2020-11-01T11:42:56Z`).
      If the timeOffset mnemonic is a floating-point value in hours, then simply use **transforms**
      and scale by 3600 seconds.  Internally, the value stored is number of seconds elapsed since
      a reference epoch, currently set to 1970-01-01T00:00:00Z.
* *(optional)* **splits** List of key value pair (splits) that define how to split the data into
  subsets of data. Any number of splits can be applied. Possible categories within each split will
  be combined to form sets which describe all unique combinations of those categories. For example
  the splits with categories ("a", "b") and ("x", "y") will be combined into four split categories
  ("a", "x"), ("a", "y"), ("b", "x"), ("b", "y").

  * **keys** are arbitrary strings (anything you want). They can be referenced in the output section.
  * **values** Type of split to apply (currently supports **category**)

    * **category** Splits data based on values assocatied with a BUFR mnemonic. Constists of:

      * **variable** The variable from the **variables** section to split on.
      * *(optional)* **map** Associates integer values in BUFR mnemonic data to a string. Please not
        that integer keys must be prepended with an **_** (ex: **_2**). Rows where where the mnemonic
        value is not defined in the map will be rejected (won't appear in output).
* *(optional)* **filters** List of filters to apply to the data before exporting. Filters exclude data
  which does not meet their requirements. The following filters are supported:

  * **bounding**

    * **variable** The variable from the *variables* section to filter on.
    * *(optional)* **upperBound** The highest possible value to accept
    * *(optional)* **lowerBound** The lowest possible value to accept

.. note::
    Either **upperBound**, **lowerBound**, or both must be present.

Encoder Description
~~~~~~~~~~~~~~~~

The **encoder** section defines the ObsGroup objects that will be created. Here is an example:

.. code-block:: yaml

  encoder:
    dimensions:
      - name: nchans
        paths:
          - "*/BRIT"
          - "*/BRITCSTC"
        source: variables/channels

    variables:
      - name: "MetaData/dateTime"
        source: "variables/timestamp"
        longName: "dateTime"
        units: "seconds since 1970-01-01T00:00:00Z"

      - name: "MetaData/latitude"
        source: "variables/latitude"
        longName: "Latitude"
        units: "degrees_north"
        range: [-90, 90]

      - name: "MetaData/longitude"
        source: "variables/longitude"
        longName: "Longitude"
        units: "degrees_east"
        range: [-180, 180]

      - name: "ObsValue/radiance"
        coordinates: "longitude latitude nchans"
        source: "variables/radiance"
        longName: "Radiance"
        units: "K"
        range: [120, 500]
        chunks: [1000, 15]
        compressionLevel: 4

* *dimensions* used to define dimension information in variables

  * **name** arbitrary name for the dimension
  * **paths** list of subqueries for that dimension (different paths for different BUFR subsets
    only) **or** *path* Single subquery for that dimension ex: **\*/BRITCSTC**
  * **source** *(optional)* The exported data that acts as the source field for this dimension.
    The data dimension values (labels) will reflect this field. The source is validated
    to make sure it makes sense for the dimension and that it is made up of repeated
    values for each occurrence of the sequence. The source field must be inside the
    dimension and be 1:1 with it.
* **variables** List of output variable objects to create.

  * **name** standardized pathname **group**/**var_name**.

    * **group** group name to which this variable belongs (example: MetaData or ObsVal).
    * **var_name** name for the variable
  * **source** reference to exported BUFR data defined in **bufr** section ex: **variables/radiance**
  * **coordinates** *(optional)*
  * **longName** any arbitrary string.
  * **units** string representing units (arbitrary but following udunits).
  * *(optional)* **range** Possible range of values (list of 2 ints).
  * *(optional)* **chunks** Size of chunked data elements ex: **[1000, 1000]**.
  * *(optional)* **compressionLevel** GZip compression level (0-9).

.. warning::
    - MetaData/dateTime **units** must be "seconds since 1970-01-01T00:00:00Z"
