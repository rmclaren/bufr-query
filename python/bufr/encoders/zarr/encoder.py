
import os
import re
from typing import Union

import zarr
import numpy as np

import bufr


# Encoder for Zarr format
class Encoder(bufr.encoders.EncoderBase):
    def __init__(self, description:Union[str, bufr.encoders.Description]):
        if isinstance(description, str):
            self.description = bufr.encoders.Description(description)
        else:
            self.description = description

        super(Encoder, self).__init__(self.description)

    def encode(self, container: bufr.DataContainer, output_path:str) -> dict[tuple[str],zarr.Group]:
        result:dict[tuple[str], zarr.Group] = {}
        for category in container.all_sub_categories():
            cat_idx = 0
            substitutions = {}
            for key in container.get_category_map().keys():
                substitutions[key] = category[cat_idx]
                cat_idx += 1

            root = zarr.open(self._make_path(output_path, substitutions), mode='w')
            # Get the dimensions for the encoder
            dims = self.get_encoder_dimensions(container, category)

            self._add_globals(root)
            self._add_dimensions(root, dims)
            self._add_datasets(root, container, category, dims)

            # Close the zarr file
            root.store.close()

            result[tuple(category)] = root

        return result

    def _add_globals(self, root:zarr.Group):
        # Adds globals as attributes to the root group
        for key, data in self.description.get_globals().items():
            root.attrs[key] = data

    def _add_dimensions(self, root:zarr.Group, dims:bufr.encoders.EncoderDimensions):
        # Add the backing variables for the dimensions
        dim_group = root.create_group('dimensions')
        for dim in dims.dims():
            dim_data = dim.labels
            dim_store = dim_group.create_dataset(dim, shape=[len(dim_data)], dtype=int)
            dim_store[:] = dim_data

    def _add_datasets(self, root:zarr.Group,
                      container: bufr.DataContainer,
                      category:[str],
                      dims:bufr.encoders.EncoderDimensions):

        for var in self.description.get_variables():
            if var["source"] not in container.list():
                raise ValueError(f'Variable {var["source"]} not found in the container')

            data = container.get(var['source'], category)
            comp_level = var['compressionLevel'] if 'compressionLevel' in var else 3

            # Create the zarr dataset
            store = root.create_dataset(var['name'],
                                        shape=data.shape,
                                        chunks=dims.chunks_for_var(var['name']),
                                        dtype=data.dtype,
                                        compression='blosc',
                                        compression_opts=dict(cname='lz4',
                                                              clevel=comp_level,
                                                              shuffle=1))
            store[:] = data

            # Add the attributes
            store.attrs['units'] = var['units']
            store.attrs['longName'] = var['longName']

            if 'coordinates' in var:
                store.attrs['coordinates'] = var['coordinates']

            if 'range' in var:
                store.attrs['valid_range'] = var['range']

            # Associate the dimensions
            store.attrs['_ARRAY_DIMENSIONS'] = dims.dim_names_for_var(var["name"])

    def _make_path(self, prototype_path:str, sub_dict:dict[str, str]):
        subs = re.findall(r'\{(?P<sub>\w+\/\w+)\}', prototype_path)
        for sub in subs:
            prototype_path = prototype_path.replace(f'{{{sub}}}', sub_dict[sub])

        return prototype_path

    def _split_source_str(self, source:str) -> (str, str):
        components = source.split('/')
        group_name = components[0]
        variable_name = components[1]

        return (group_name, variable_name)
