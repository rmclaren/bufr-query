import bufr

from ..encoders import netcdf, zarr

from .logger import *

FILE_ENCODER_DICT = {'netcdf': netcdf.Encoder,
                     'zarr': zarr.Encoder}

def addEncoderType(name, encoder):
    FILE_ENCODER_DICT[name] = encoder

class ObsBuilder:
    def __init__(self, input_path, mapping_path):
        self.input_path = input_path
        self.mapping_path = mapping_path

    # Virtual Method
    def make_description(self) -> bufr.encoders.Description:
        return bufr.encoders.Description(self.mapping_path)

    # Virtual Method
    def make_obs(self, comm) -> bufr.DataContainer:
        return bufr.Parser(self.input_path, self.mapping_path).parse(comm)

    def create_obs_group(self, category, env):
        from pyioda.ioda.Engines.Bufr import Encoder as iodaEncoder

        comm = bufr.mpi.Comm(env["comm_name"])

        # Check the cache for the data and return it if it exists
        logging(comm, 'DEBUG', f'Check if bufr.DataCache exists? {bufr.DataCache.has(self.input_path, self.mapping_path)}')
        if bufr.DataCache.has(self.input_path, self.mapping_path):
            container = bufr.DataCache.get(self.input_path, self.mapping_path)
            logging(comm, 'INFO', f'Encode {category} from cache')
            data = iodaEncoder(self.make_description()).encode(container)[(category,)]
            logging(comm, 'INFO', f'Mark {category} as finished in the cache')
            bufr.DataCache.mark_finished(self.input_path, self.mapping_path, [category])
            logging(comm, 'INFO', f'Return the encoded data for {category}')
            return data

        container = self.make_obs(comm)

        # Gather data from all tasks into all tasks. Each task will have the complete record
        logging(comm, 'INFO', f'Gather data from all tasks into all tasks')
        container.all_gather(comm)

        logging(comm, 'INFO', f'Add container to cache')
        # Add the container to the cache
        bufr.DataCache.add(self.input_path, self.mapping_path, container.all_sub_categories(), container)

        # Encode the data
        logging(comm, 'INFO', f'Encode {category}')
        data = iodaEncoder(self.make_description()).encode(container)[(category,)]

        logging(comm, 'INFO', f'Mark {category} as finished in the cache')
        # Mark the data as finished in the cache
        bufr.DataCache.mark_finished(self.input_path, self.mapping_path, [category])

        logging(comm, 'INFO', f'Return the encoded data for {category}')
        return data

    def create_obs_file(self, output_path, type='netcdf', append=False):

        comm = bufr.mpi.Comm("world")
        container = self.make_obs(comm)
        container.gather(comm)

        # Encode the data
        if comm.rank() == 0:
            FILE_ENCODER_DICT[type](self.make_description()).encode(container, output_path, append)

        logging(comm, 'INFO', f'Return the encoded data')
