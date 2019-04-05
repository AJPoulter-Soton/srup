//
// Created by AJ Poulter on 14/07/2017.
//

#include "SRUP_Data.h"

SRUP_MSG_DATA::SRUP_MSG_DATA()
{
    m_msgtype[0] = SRUP::SRUP_MESSAGE_TYPE_DATA;
    m_data = nullptr;
    m_data_ID = nullptr;
    m_data_ID_len = 0;
    m_data_len = 0;
}

SRUP_MSG_DATA::~SRUP_MSG_DATA()
{
    if (m_data != nullptr)
        delete (m_data);
    if (m_data_ID != nullptr)
        delete (m_data_ID);
}

bool SRUP_MSG_DATA::data_ID(const uint8_t *data_ID, const uint16_t len)
{
    m_is_serialized = false;

    if (m_data_ID != nullptr)
        delete(m_data_ID);

    m_data_ID = new uint8_t[len+1];
    std::memcpy(m_data_ID, data_ID, len);
    std::memset(m_data_ID+len, 0, 1);
    m_data_ID_len = len;
    return true;
}

const uint8_t *SRUP_MSG_DATA::data_ID()
{
    return m_data_ID;
}


unsigned char *SRUP_MSG_DATA::Serialized()
{
    if (Serialize())
        return m_serialized;
    else
        return nullptr;
}

bool SRUP_MSG_DATA::DeSerialize(const uint8_t *serial_data)
{
    uint16_t x;
    uint32_t p=0;
    uint8_t bytes[2];

    // We need to unmarshall the data to reconstruct the object...
    // We can start with the two bytes for the header.
    // One for the version - and one for the message type.

    std::memcpy(m_version, (uint8_t*) serial_data, 1);
    p+=1;
    std::memcpy(m_msgtype, (uint8_t*) serial_data + p, 1);
    p+=1;

    // Now we have to unmarshall the sequence ID...
    uint8_t sid_bytes[8];
    for (int i=0;i<8;i++)
    {
        std::memcpy(&sid_bytes[i], (uint8_t*) serial_data + p, 1);
        ++p;
    }

    // ... then we copy them into m_sequence_ID
    if (m_sequence_ID != nullptr)
        delete(m_sequence_ID);
    m_sequence_ID = new uint64_t;
    std::memcpy(m_sequence_ID, sid_bytes, 8);

    // Next we have to unmarshall the sender ID...
    uint8_t snd_bytes[8];
    for (int i=0;i<8;i++)
    {
        std::memcpy(&snd_bytes[i], (uint8_t*) serial_data + p, 1);
        ++p;
    }

    // ... then we copy them into the sender ID
    if (m_sender_ID != nullptr)
        delete(m_sender_ID);
    m_sender_ID = new uint64_t;
    std::memcpy(m_sender_ID, snd_bytes, 8);

    // Now we have two-bytes for the size of the token ... and x bytes for the token
    std::memcpy(bytes, serial_data + p, 2);
    x = decodeLength(bytes);
    p+=2;
    if(m_token != nullptr)
        delete(m_token);
    m_token = new uint8_t[x+1];
    std::memcpy(m_token, (uint8_t *) serial_data + p, x);
    m_token_len = x;
    p+=x;

    // The next two bytes are the size of the signature...
    std::memcpy(bytes, serial_data + p, 2);
    x = decodeLength(bytes);
    p+=2;

    m_sig_len = x;

    // The next x bytes are the value of the signature.
    if(m_signature != nullptr)
        delete(m_signature);
    m_signature = new unsigned char[x];
    std::memcpy(m_signature, serial_data + p, x);

    p+=x;

    // Next the data_ID...
    std::memcpy(bytes, serial_data + p, 2);
    x = decodeLength(bytes);
    p+=2;
    if(m_data_ID != nullptr)
        delete(m_data_ID);
    m_data_ID = new uint8_t[x+1];
    std::memcpy(m_data_ID, (uint8_t *) serial_data + p, x);
    m_data_ID_len = x;
    p+=x;

    // ...and lastly the data
    std::memcpy(bytes, serial_data + p, 2);
    x = decodeLength(bytes);
    p+=2;
    if(m_data != nullptr)
        delete(m_data);
    m_data = new uint8_t[x+1];
    std::memcpy(m_data, (uint8_t *) serial_data + p, x);
    m_data_len = x;

    return true;
}

uint32_t SRUP_MSG_DATA::SerializedLength()
{
    if (!m_is_serialized)
        Serialize();

    return m_serial_length;
}

bool SRUP_MSG_DATA::Serialize(bool preSign)
{
    // As we're using dynamic memory - and only storing / sending the length of the data we have
    // we need to know how long all of the fields are so that we can unmarshall the data at
    // the other end...

    const uint16_t header_size = 18; // Two-bytes for the main header - plus 8 for the sequence ID & 8 for the sender ID..
    const uint8_t field_length_size = 2;

    // We need the number of variable length fields - including the m_sig_len...
    // ... this can't be const as we need to change it depending on whether or not we're in pre-sign.
    // (If we are we need to reduce it by one for the unused m_sig_len)
    // We have the signature, the token, the data_ID & the data...
    uint8_t var_length_field_count = 4;

    uint32_t serial_len;
    uint32_t p=0;

    uint8_t * msb;
    uint8_t * lsb;

    if (m_token == nullptr)
        return false;

    msb=new uint8_t[1];
    lsb=new uint8_t[1];

    // Now check that we have a sequence ID...
    if (m_sequence_ID == nullptr)
    {
        delete[] msb;
        delete[] lsb;
        return false;
    }

    // ...and check that we have a sender ID
    if (m_sender_ID == nullptr)
    {
        delete[] msb;
        delete[] lsb;
        return false;
    }

    // If we're calling this as a prelude to signing / verifying then we need to exclude the signature data from the
    // serial data we generate...

    if (preSign)
    {
        serial_len = m_token_len + m_data_ID_len + m_data_len;
        var_length_field_count--;
    }
    else
        serial_len = m_sig_len + m_token_len + m_data_ID_len + m_data_len;

    m_serial_length = serial_len + header_size + (field_length_size * var_length_field_count);

    if (m_serialized != nullptr)
        delete (m_serialized);

    m_serialized = new uint8_t[m_serial_length];
    std::memset(m_serialized, 0, m_serial_length);

    // The first two fields are fixed length (1 byte each).
    std::memcpy(m_serialized, m_version, 1);
    p+=1;
    std::memcpy(m_serialized + p, m_msgtype, 1);
    p+=1;

    // Now we need to add the Sequence ID (uint64_t)
    // See SRUP_Init.cpp for details...
    for (int x=0;x<8;x++)
    {
        uint8_t byte;
        byte = getByteVal(*m_sequence_ID, x);
        std::memcpy(m_serialized + p, &byte, 1);
        p+=1;
    }

    // And we need to do the same thing for the Sender ID (uint64_t)
    for (int x=0;x<8;x++)
    {
        uint8_t byte;
        byte = getByteVal(*m_sender_ID, x);
        std::memcpy(m_serialized + p, &byte, 1);
        p+=1;
    }

    // All of the other fields need their length to be specified...

    encodeLength(lsb, msb, m_token_len);
    std::memcpy(m_serialized + p, msb, 1);
    p+=1;
    std::memcpy(m_serialized + p, lsb, 1);
    p+=1;
    std::memcpy(m_serialized + p, m_token, m_token_len);
    p+=m_token_len;

    // If we're executing Serialize as a part of generating the signature - we can't marshall the signature
    // as we haven't calculated it yet. So only do the signature if we're not in preSign

    if (!preSign)
    {
        if (m_signature == nullptr)
        {
            delete[] msb;
            delete[] lsb;
            return false;
        }
        else
        {
            if (m_sig_len == 0)
            {
                delete[] msb;
                delete[] lsb;
                return false;
            }
            else
            {
                encodeLength(lsb, msb, m_sig_len);
                std::memcpy(m_serialized + p, msb, 1);
                p += 1;
                std::memcpy(m_serialized + p, lsb, 1);
                p += 1;
                std::memcpy(m_serialized + p, m_signature, m_sig_len);
                p += m_sig_len;
            }
        }
    }

    encodeLength(lsb, msb, m_data_ID_len);
    std::memcpy(m_serialized + p, msb, 1);
    p+=1;
    std::memcpy(m_serialized + p, lsb, 1);
    p+=1;
    std::memcpy(m_serialized + p, m_data_ID, m_data_ID_len);
    p+=m_data_ID_len;

    encodeLength(lsb, msb, m_data_len);
    std::memcpy(m_serialized + p, msb, 1);
    p+=1;
    std::memcpy(m_serialized + p, lsb, 1);
    p+=1;
    std::memcpy(m_serialized + p, m_data, m_data_len);
    p+=m_data_len;

    delete[] msb;
    delete[] lsb;

    // If we're in preSign we don't have a real value for m_serialized - so copy the data to m_unsigned_message
    // and discard (and reset) m_serialized & m_serial_length...
    if (preSign)
    {
        if (m_unsigned_message != nullptr)
            delete(m_unsigned_message);
        m_unsigned_message = new unsigned char[m_serial_length];

        std::memcpy(m_unsigned_message, m_serialized, m_serial_length);
        m_unsigned_length = m_serial_length;

        m_serial_length = 0;
        delete (m_serialized);
        m_serialized= nullptr;
    }

    m_is_serialized = true;
    return true;

}

bool SRUP_MSG_DATA::DataCheck()
{
    if ((m_data != nullptr) && (m_data_ID != nullptr) && (m_token != nullptr) && (m_sequence_ID != nullptr) && (m_sender_ID != nullptr))
       return true;
    else
        return false;
}

bool SRUP_MSG_DATA::data(const uint8_t *data, const uint16_t len)
{
    m_is_serialized = false;

    if (m_data != nullptr)
        delete(m_data);

    m_data = new uint8_t[len];
    std::memcpy(m_data, data, len);
    m_data_len = len;
    return true;
}

bool SRUP_MSG_DATA::data(const uint8_t data)
{
    m_is_serialized = false;
    const uint8_t len = 1;

    if (m_data != nullptr)
        delete(m_data);

    m_data = new uint8_t;
    std::memcpy(m_data, &data, len);
    m_data_len = len;
    return true;
}

bool SRUP_MSG_DATA::data(const int8_t data)
{
    m_is_serialized = false;
    const uint8_t len = 1;

    if (m_data != nullptr)
        delete(m_data);

    m_data = new uint8_t;
    std::memcpy(m_data, &data, len);
    m_data_len = len;
    return true;
}

bool SRUP_MSG_DATA::data(const uint16_t data)
{
    m_is_serialized = false;
    const uint8_t len = 2;

    if (m_data != nullptr)
        delete(m_data);

    m_data = new uint8_t[len];
    std::memcpy(m_data, &data, len);
    m_data_len = len;
    return true;
}

bool SRUP_MSG_DATA::data(const int16_t data)
{
    m_is_serialized = false;
    const uint8_t len = 2;

    if (m_data != nullptr)
        delete(m_data);

    m_data = new uint8_t[len];
    std::memcpy(m_data, &data, len);
    m_data_len = len;
    return true;
}

bool SRUP_MSG_DATA::data(const uint32_t data)
{
    m_is_serialized = false;
    const uint8_t len = 4;

    if (m_data != nullptr)
        delete(m_data);

    m_data = new uint8_t[len];
    std::memcpy(m_data, &data, len);
    m_data_len = len;
    return true;
}

bool SRUP_MSG_DATA::data(const int32_t data)
{
    m_is_serialized = false;
    const uint8_t len = 4;

    if (m_data != nullptr)
        delete(m_data);

    m_data = new uint8_t[len];
    std::memcpy(m_data, &data, len);
    m_data_len = len;
    return true;
}
bool SRUP_MSG_DATA::data(const uint64_t data)
{
    m_is_serialized = false;
    const uint8_t len = 8;

    if (m_data != nullptr)
        delete(m_data);

    m_data = new uint8_t[len];
    std::memcpy(m_data, &data, len);
    m_data_len = len;
    return true;
}

bool SRUP_MSG_DATA::data(const int64_t data)
{
    m_is_serialized = false;
    const uint8_t len = 8;

    if (m_data != nullptr)
        delete(m_data);

    m_data = new uint8_t[len];
    std::memcpy(m_data, &data, len);
    m_data_len = len;
    return true;
}

bool SRUP_MSG_DATA::data(const float data)
{
    m_is_serialized = false;
    const uint8_t len = 4;

    if (m_data != nullptr)
        delete(m_data);

    m_data = new uint8_t[len];
    std::memcpy(m_data, &data, len);
    m_data_len = len;
    return true;
}

bool SRUP_MSG_DATA::data(const double data)
{
    m_is_serialized = false;
    const uint8_t len = 8;

    if (m_data != nullptr)
        delete(m_data);

    m_data = new uint8_t[len];
    std::memcpy(m_data, &data, len);
    m_data_len = len;
    return true;
}

const uint8_t *SRUP_MSG_DATA::data()
{
    return m_data;
}

uint8_t* SRUP_MSG_DATA::data_uint8()
{
    return m_data;
}

int8_t* SRUP_MSG_DATA::data_int8()
{
    return (int8_t*) m_data;
}

uint16_t* SRUP_MSG_DATA::data_uint16()
{
    return (uint16_t*) m_data;
}
int16_t* SRUP_MSG_DATA::data_int16()
{
    return (int16_t*) m_data;
}

uint32_t* SRUP_MSG_DATA::data_uint32()
{
    return (uint32_t*) m_data;
}

int32_t* SRUP_MSG_DATA::data_int32()
{
    return (int32_t*) m_data;
}

uint64_t* SRUP_MSG_DATA::data_uint64()
{
    return (uint64_t*) m_data;
}

int64_t* SRUP_MSG_DATA::data_int64()
{
    return (int64_t*) m_data;
}

float* SRUP_MSG_DATA::data_float()
{
    return (float*) m_data;
}

double* SRUP_MSG_DATA::data_double()
{
    return (double *) m_data;
}

uint16_t SRUP_MSG_DATA::data_ID_length()
{
    return m_data_ID_len;
}

uint16_t SRUP_MSG_DATA::data_length()
{
    return m_data_len;
}
